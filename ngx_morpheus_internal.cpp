/*
* Copyright (C) ab 11/29/21
*/

#include <iostream>
#include <sstream>
#include <string>
#include <regex>
#include <chrono>
#include "pugixml.hpp"

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
    const char* ckmmdata;

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

void morph_process(const char* encmpd, const char* drmconf, const char* iframesmpd) {
    /* if iframes track mpd exists add its' AdaptationSet first
     * then if drmconf exists, add drm pieces
     * then do the other modifications to the mpd
     */
    pugi::xml_document doc;
    doc.load_file((const char*)encmpd);

    if (iframesmpd)
        morph_iframes(doc, iframesmpd);

    if (drmconf)
        morph_drm(doc, drmconf);

    pugi::xml_node mpd = doc.child("MPD");

    //add suggestedPresentationDelay
    pugi::xml_attribute presdel = mpd.append_attribute("suggestedPresentationDelay");
    presdel.set_value("15");

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

