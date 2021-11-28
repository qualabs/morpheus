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

void morph_process (const char* tempdata) {
    /* load tempdata
     * change the conents
     * save tempdata
     */
    pugi::xml_document doc;
    doc.load_file((const char*)tempdata);

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
        if ( val == "progressive")
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

    doc.save_file((const char*)tempdata);
}

#ifdef __cplusplus
}
#endif


