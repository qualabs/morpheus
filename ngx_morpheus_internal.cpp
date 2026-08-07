/*
* Copyright (C) ab 11/29/21
*/

#include <iostream>
#include <sstream>
#include <string>
#include <regex>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <vector>
#include "pugixml.hpp"
#include <unordered_map>

const std::unordered_map<int, std::string> MANIFEST_URLS = {
    {1, "http://localhost:3000/api/list-mpd?vasturl=http://localhost:3000/samples/dash-alt-mpd/vast-sample.xml"},
    {2, "https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch2/real/b/ad-insertion-testcase1.mpd"},
    {3, "https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch2/real/b/ad-insertion-testcase1.mpd"}
};

// Overlay ad URLs are configurable at runtime via environment variables, with
// safe fallback to the compiled defaults below if the env var is unset/empty.
//   final url = ${AD_GEN_BASE_URL}/image/html?${MORPHEUS_<SHAPE>_QUERY}
// The base default is intentionally neutral/portable (localhost, not a LAN IP).
#define DEFAULT_AD_GEN_BASE_URL  "http://localhost:8888"

// Per-shape fallback query strings (everything after "image/html?"), using the
// repo's original template_ids. Personalization params are intentionally not
// hardcoded: they flow in client-side from the MPD URL query (ExtUrlQueryInfo).
#define DEFAULT_BANNER_QUERY        "template_id=17306275-3762-45f9-9a86-c262b8925963"
#define DEFAULT_SKYSCRAPER_QUERY    "template_id=cda83e2d-0cd9-44f6-b1d5-d9c0c62cc203"
#define DEFAULT_LSHAPE_RIGHT_QUERY  "template_id=7822830e-10ff-449f-a5e6-f92f7899f442"
#define DEFAULT_LSHAPE_LEFT_QUERY   "template_id=1ac069f1-0437-4a9d-861b-e29ad552c842"

static std::string fmt_double(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

struct vec2 { double x, y; };
struct squeeze_cfg { bool active; double pct; const char* origin; };
struct shape_cfg { const char* query_env; const char* query_default; int z; vec2 viewport, size, top_left; squeeze_cfg squeeze; };

static const shape_cfg SHAPE_CONFIGS[] = {
    // QUERY_ENV                       QUERY_DEFAULT               Z  VIEWPORT      SIZE          TOPLEFT     SQUEEZE
    { "MORPHEUS_BANNER_QUERY",        DEFAULT_BANNER_QUERY,        1, {1920, 1080}, {1920, 324},  {0, 756},   {false, 0.0, ""} },
    { "MORPHEUS_SKYSCRAPER_QUERY",    DEFAULT_SKYSCRAPER_QUERY,    1, {1920, 1080}, {480, 1080},  {0, 0},     {false, 0.0, ""} },
    { "MORPHEUS_LSHAPE_RIGHT_QUERY",  DEFAULT_LSHAPE_RIGHT_QUERY, -1, {1.0,1.0},    {1.0,1.0},    {0.0,0.0},  {true,  0.6, "top left"} },
    { "MORPHEUS_LSHAPE_LEFT_QUERY",   DEFAULT_LSHAPE_LEFT_QUERY,  -1, {1.0,1.0},    {1.0,1.0},    {0.0,0.0},  {true,  0.6, "top right"} },
};

// Build the overlay ad URL for a shape from env vars, falling back to the
// compiled defaults when an env var is unset or empty.
static std::string build_ad_url(const shape_cfg& cfg) {
    const char* base = std::getenv("AD_GEN_BASE_URL");
    if (!base || !base[0]) base = DEFAULT_AD_GEN_BASE_URL;
    const char* query = std::getenv(cfg.query_env);
    if (!query || !query[0]) query = cfg.query_default;
    std::string b(base);
    // avoid double slash if base ends with '/'
    if (!b.empty() && b.back() == '/') b.pop_back();
    return b + "/image/html?" + query;
}

static int parse_shape(const char* upid_text) {
    char buf[256] = {};
    std::strncpy(buf, upid_text, sizeof(buf) - 1);

    // trim leading whitespace
    char* p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;

    // trim trailing whitespace and '&'
    char* end = p + std::strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '&'))
        *end-- = '\0';

    // normalize to lowercase
    for (char* c = p; *c; ++c)
        *c = (char)std::tolower((unsigned char)*c);

    if (std::strncmp(p, "shape=", 6) != 0) return -1;
    const char* val = p + 6;

    if (std::strcmp(val, "banner")       == 0) return 0;
    if (std::strcmp(val, "skyscraper")   == 0) return 1;
    if (std::strcmp(val, "lshape-right") == 0) return 2;
    if (std::strcmp(val, "lshape-left")  == 0) return 3;
    return -1;
}

struct overlay_ev {
    bool is_start;
    uint32_t seg_id;
    uint64_t ptime;
    uint64_t dur;
    int shape_idx;
};

void morph_overlay(pugi::xml_document& mpddoc) {
    pugi::xml_node mpd = mpddoc.child("MPD");

    for (pugi::xml_node period : mpd.children("Period")) {
        pugi::xml_node scte_stream;
        for (pugi::xml_node es : period.children("EventStream")) {
            std::string scheme = es.attribute("schemeIdUri").value();
            if (scheme.find("scte35") != std::string::npos) {
                scte_stream = es;
                break;
            }
        }
        if (!scte_stream) continue;

        // Only process EventStreams that carry SCTE-35 segmentation overlay events
        bool has_segmentation = false;
        for (pugi::xml_node ev : scte_stream.children("Event")) {
            pugi::xml_node sec = ev.child("scte35:SpliceInfoSection");
            if (!sec) continue;
            pugi::xml_node desc = sec.child("scte35:SegmentationDescriptor");
            if (!desc) continue;
            int type_id = desc.attribute("segmentationTypeId").as_int(0);
            if (type_id == 56 || type_id == 57) { has_segmentation = true; break; }
        }
        if (!has_segmentation) continue;

        uint64_t timescale = scte_stream.attribute("timescale").as_ullong(1000);
        std::vector<overlay_ev> events;

        for (pugi::xml_node event : scte_stream.children("Event")) {
            uint64_t ptime = event.attribute("presentationTime").as_ullong(0);
            uint64_t dur   = event.attribute("duration").as_ullong(0);

            pugi::xml_node section    = event.child("scte35:SpliceInfoSection");
            if (!section) continue;
            pugi::xml_node descriptor = section.child("scte35:SegmentationDescriptor");
            if (!descriptor) continue;

            int type_id = descriptor.attribute("segmentationTypeId").as_int(0);

            const char* seg_id_str = descriptor.attribute("segmentationEventId").value();
            uint32_t seg_id = seg_id_str[0]
                ? (uint32_t)std::strtoul(seg_id_str, nullptr, 0)
                : event.attribute("id").as_uint(0);

            if (type_id == 56) {
                const char* upid_text = nullptr;
                for (pugi::xml_node upid : descriptor.children("scte35:SegmentationUpid")) {
                    if (upid.attribute("segmentationUpidType").as_int(0) == 14) {
                        upid_text = upid.child_value();
                        break;
                    }
                }
                if (!upid_text || !upid_text[0]) {
                    std::cerr << "morpheus: overlay start event id=" << seg_id << " missing UPID type 14, skipping\n";
                    continue;
                }
                int shape_idx = parse_shape(upid_text);
                if (shape_idx < 0) {
                    std::cerr << "morpheus: overlay start event id=" << seg_id << " unknown shape, skipping\n";
                    continue;
                }
                events.push_back({true, seg_id, ptime, dur, shape_idx});

            } else if (type_id == 57) {
                events.push_back({false, seg_id, ptime, 0, -1});
            }
        }

        // Remove SCTE-35 Event children, preserving any SupplementalProperty children
        std::vector<pugi::xml_node> evts_to_remove;
        for (pugi::xml_node ev : scte_stream.children("Event")) {
            evts_to_remove.push_back(ev);
        }
        for (auto& ev : evts_to_remove) {
            scte_stream.remove_child(ev);
        }

        // Convert to overlay EventStream in place
        scte_stream.attribute("schemeIdUri").set_value("urn:scte:dash:scte214-events");

        for (const overlay_ev& oe : events) {
            pugi::xml_node ev = scte_stream.append_child("Event");
            ev.append_attribute("presentationTime").set_value((unsigned long long)oe.ptime);

            if (oe.is_start) {
                if (oe.dur > 0)
                    ev.append_attribute("duration").set_value((unsigned long long)oe.dur);
                ev.append_attribute("id").set_value((unsigned long)oe.seg_id);

                const shape_cfg& cfg = SHAPE_CONFIGS[oe.shape_idx];

                pugi::xml_node overlay = ev.append_child("OverlayEvent");
                std::string ad_url = build_ad_url(cfg);
                overlay.append_attribute("uri").set_value(ad_url.c_str());
                overlay.append_attribute("mimeType").set_value("text/html");
                overlay.append_attribute("earliestResolutionTime").set_value("35000");
                overlay.append_attribute("loop").set_value("false");
                overlay.append_attribute("mode").set_value("start");
                overlay.append_attribute("z").set_value(cfg.z);

                overlay.append_child("Viewport")
                    .append_attribute("x").set_value(fmt_double(cfg.viewport.x).c_str());
                overlay.child("Viewport")
                    .append_attribute("y").set_value(fmt_double(cfg.viewport.y).c_str());

                overlay.append_child("Size")
                    .append_attribute("x").set_value(fmt_double(cfg.size.x).c_str());
                overlay.child("Size")
                    .append_attribute("y").set_value(fmt_double(cfg.size.y).c_str());

                overlay.append_child("TopLeft")
                    .append_attribute("x").set_value(fmt_double(cfg.top_left.x).c_str());
                overlay.child("TopLeft")
                    .append_attribute("y").set_value(fmt_double(cfg.top_left.y).c_str());

                if (cfg.squeeze.active) {
                    pugi::xml_node sq = overlay.append_child("SqueezeCurrent");
                    sq.append_attribute("percentage").set_value(fmt_double(cfg.squeeze.pct).c_str());
                    sq.append_attribute("origin").set_value(cfg.squeeze.origin);
                }
            } else {
                pugi::xml_node overlay = ev.append_child("OverlayEvent");
                overlay.append_attribute("mode").set_value("stop");
                overlay.append_attribute("refId").set_value((unsigned long)oe.seg_id);
            }
        }

        // If stream-lens injected a urlparam SP, augment it with useMPDUrlQuery.
        // Otherwise create morpheus's own SP.
        pugi::xml_node existing_sp;
        for (pugi::xml_node sp : scte_stream.children("SupplementalProperty")) {
            if (std::strcmp(sp.attribute("schemeIdUri").value(), "urn:mpeg:dash:urlparam:2016") == 0) {
                existing_sp = sp;
                break;
            }
        }

        if (existing_sp) {
            pugi::xml_node eqi = existing_sp.child("up:ExtUrlQueryInfo");
            if (eqi) {
                eqi.append_attribute("useMPDUrlQuery").set_value("true");
            }
        } else {
            pugi::xml_node sp = scte_stream.append_child("SupplementalProperty");
            sp.append_attribute("schemeIdUri").set_value("urn:mpeg:dash:urlparam:2016");
            pugi::xml_node eqi = sp.append_child("up:ExtUrlQueryInfo");
            eqi.append_attribute("xmlns:up").set_value("urn:mpeg:dash:schema:urlparam:2016");
            eqi.append_attribute("useMPDUrlQuery").set_value("true");
            eqi.append_attribute("queryTemplate").set_value("$querypart$");
            eqi.append_attribute("includeInRequests").set_value("urn:scte:dash:scte214-events");
        }
    }
}

#ifdef __cplusplus
extern "C" {
#endif


void morph_iframes(pugi::xml_document& mpddoc, const char* iframesmpd) {
    /* take the first adaptationset in iframesmpd
    *  and put it in mpddoc
    */
    pugi::xml_document iframesdoc;

    iframesdoc.load_file((const char*)iframesmpd);
    pugi::xml_node iframe_adset = iframesdoc.child("MPD").child("Period").child("AdaptationSet");
    pugi::xml_node adsets = mpddoc.child("MPD").child("Period");

    adsets.append_copy(iframe_adset);
}

void morph_drm(pugi::xml_document& mpddoc, const char* drmconf) {

    pugi::xml_document drmdoc;
    drmdoc.load_file((const char*)drmconf);

    /* from drmconf need:
    *    <iv>
    *    <drm-key-id>
    *    <attribute-list> <attribute key="ckmMetaData">
    */
    pugi::xml_node ns2 = drmdoc.child("ns2:rck");
    const char* iv = ns2.child_value("iv");
    const char* drmkeyid = ns2.child_value("drm-key-id");
    const char* ckmmdata = "temp";

    pugi::xml_node atrblist = ns2.child("attribute-list");
    for (pugi::xml_node atrb = atrblist.first_child(); atrb; atrb = atrb.next_sibling())
    {
        std::string key = atrb.attribute("key").value();
        if (key == "ckmMetaData")
        {
            ckmmdata = atrb.child_value();
            break;
        }
    }

    pugi::xml_node adsets = mpddoc.child("MPD").child("Period");

    for (pugi::xml_node adset = adsets.first_child(); adset; adset = adset.next_sibling())
    {
        pugi::xml_node conprot = adset.prepend_child("ContentProtection");
        pugi::xml_node segenc = conprot.append_child("SegmentEncryption");
        pugi::xml_node cryper = conprot.append_child("CryptoPeriod");

        pugi::xml_attribute cryper_iv = cryper.append_attribute("IV");
        cryper_iv.set_value(iv);

        pugi::xml_attribute cryper_kut = cryper.append_attribute("keyUriTemplate");
        std::string kut_string = std::string("tag:car.comcast.com,2016:ckm/clk/drm/none/drmkid/").append(drmkeyid);
        kut_string = kut_string.append("/ckmmetadata/");
        kut_string = kut_string.append(ckmmdata);

        cryper_kut.set_value(kut_string.c_str());

        pugi::xml_attribute segenc_siu = segenc.append_attribute("schemeIdUri");
        segenc_siu.set_value("urn:mpeg:dash:sea:cenc-cbcs:2016");

        pugi::xml_attribute conprot_siu = conprot.append_attribute("schemeIdUri");
        conprot_siu.set_value("urn:mpeg:dash:sea:2013");
    }
}

void morph_alternative(pugi::xml_document& mpddoc) {
    pugi::xml_node mpd = mpddoc.child("MPD");

    for (pugi::xml_node period : mpd.children("Period")) {
        for (pugi::xml_node event_stream : period.children("EventStream")) {

            std::string scheme_id = event_stream.attribute("schemeIdUri").value();
            if (scheme_id != "urn:scte:scte35:2013:xml") continue;

            // Only claim streams that actually contain SpliceInsert events
            bool has_splice_insert = false;
            for (pugi::xml_node ev : event_stream.children("Event")) {
                pugi::xml_node sec = ev.child("scte35:SpliceInfoSection");
                if (sec && sec.child("scte35:SpliceInsert")) { has_splice_insert = true; break; }
            }
            if (!has_splice_insert) continue;

            event_stream.attribute("schemeIdUri").set_value("urn:mpeg:dash:event:alternativeMPD:replace:2025");

            if (!event_stream.attribute("xmlns")) {
                event_stream.append_attribute("xmlns").set_value("");
            }

            for (pugi::xml_node event : event_stream.children("Event")) {

                uint64_t presentationTime = event.attribute("presentationTime").as_ullong(0);
                uint64_t duration = event.attribute("duration").as_ullong(0);

                pugi::xml_node scte_section = event.child("scte35:SpliceInfoSection");
                if (scte_section) {
                    pugi::xml_node splice_insert = scte_section.child("scte35:SpliceInsert");

                    if (splice_insert) {
                        int splice_event_id = splice_insert.attribute("spliceEventId").as_int(1);

                        pugi::xml_node break_duration = splice_insert.child("scte35:BreakDuration");

                        uint64_t scte_duration = break_duration.attribute("duration").as_ullong(0);

                        event.remove_child(scte_section);

                        pugi::xml_node replace_presentation = event.append_child("ReplacePresentation");

                        auto it = MANIFEST_URLS.find(splice_event_id);
                        if (it == MANIFEST_URLS.end()) {
                            throw std::runtime_error("Manifest URL not found for spliceEventId: " + std::to_string(splice_event_id));
                        }
                        std::string manifest_url = it->second;

                        replace_presentation.append_attribute("url").set_value(manifest_url.c_str());
                        replace_presentation.append_attribute("earliestResolutionTimeOffset").set_value(std::to_string(presentationTime).c_str());
                        replace_presentation.append_attribute("returnOffset").set_value(std::to_string(duration).c_str());
                        replace_presentation.append_attribute("maxDuration").set_value(std::to_string(scte_duration).c_str());
                        replace_presentation.append_attribute("clip").set_value("false");
                        replace_presentation.append_attribute("startAtPlayhead").set_value("false");
                    }
                }
            }
        }
    }
}

void morph_process(const char* encmpd, const char* drmconf, const char* iframesmpd) {
    pugi::xml_document doc;
    doc.load_file((const char*)encmpd);

    if (iframesmpd)
        morph_iframes(doc, iframesmpd);

    if (drmconf)
        morph_drm(doc, drmconf);

    morph_alternative(doc);
    morph_overlay(doc);

    pugi::xml_node mpd = doc.child("MPD");

    //add suggestedPresentationDelay
    pugi::xml_attribute presdel = mpd.attribute("suggestedPresentationDelay");
    if (!presdel) {
        presdel = mpd.append_attribute("suggestedPresentationDelay");
        presdel.set_value("15");
    }

    //add our own UTC timing descriptor
    mpd.remove_child("UTCTiming");
    pugi::xml_node utc = mpd.append_child("UTCTiming");

    pugi::xml_attribute siu = utc.append_attribute("schemeIdUri");
    siu.set_value("urn:mpeg:dash:utc:direct:2014");

    pugi::xml_attribute val = utc.append_attribute("value");
    
    //much simpler code for 1 second precision
    /*
    time_t now;
    time(&now);
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
    */

    //millisecond precision
    using namespace std::chrono;
    system_clock::time_point now = system_clock::now();
    time_t timet = system_clock::to_time_t(now);
    std::tm tm{};

    std::string format = std::string(u8"%FT%T.").append(std::to_string(duration_cast<milliseconds>(now.time_since_epoch()).count() % static_cast<long long>(1000)));

    gmtime_r(&timet, &tm);
    std::string res = std::string(255, 0);
    const size_t length = std::strftime(&res[0], res.size(), format.c_str(), &tm);
    res.resize(length);

    val.set_value(res.c_str());

    //remove scanType if value is progressive
    pugi::xpath_query query_scan_type("/MPD/Period/AdaptationSet/Representation");
    pugi::xpath_node_set scan_items = query_scan_type.evaluate_node_set(doc);
    for (pugi::xpath_node_set::const_iterator it = scan_items.begin(); it != scan_items.end(); ++it)
    {
        pugi::xpath_node node = *it;
        std::string val = node.node().attribute("scanType").value();
        if (val == "progressive")
            node.node().remove_attribute("scanType");
    }

    //remove startNumber if no $Number$ 
    const std::regex reg("\\$Number.*\\$");
    pugi::xpath_query query_remote_timelines("/MPD/Period/AdaptationSet/SegmentTemplate");
    pugi::xpath_node_set items = query_remote_timelines.evaluate_node_set(doc);
    for (pugi::xpath_node_set::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        pugi::xpath_node node = *it;
        if ( node.node().attribute("media") )
        {
            std::string val = node.node().attribute("media").value();
            if ( std::regex_search(val, reg) )
                continue;
            //the SegmentTemplate does not include the $Number$
            //string, remove the startNumber attribute
            node.node().remove_attribute("startNumber");
        }
    }

    doc.save_file((const char*)encmpd);
}

#ifdef __cplusplus
}
#endif

