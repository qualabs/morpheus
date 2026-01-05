#include <iostream>
#include "cxxopts.hpp"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

void morph_process(const char* encmpd, const char* drmconf, const char* iframesmpd, const bool alternativeconf);

int main (int argc, char *argv[]) {

    string inmpd_o, drmconf_o, iframesmpd_o;
    bool alternativeconf_o = false;

    try
    {
        cxxopts::Options options(argv[0], "morpheus VOD stand-alone tool");

        options.set_width(80).add_options()
            ("n", "encoder mpd file", cxxopts::value<string>())
            ("i", "iframes track mpd file", cxxopts::value<string>())
            ("d", "ckm encrypt context response xml file", cxxopts::value<string>())
            ("a", "alterantive insertion")
            ("h,help", "Print this help")
            ;

        auto result = options.parse(argc, argv);

        if ( result.count("help") || argc == 1 )
        {
            cout << options.help() << endl;
            exit(0);
        }

        if (result.count("n"))
            inmpd_o = result["n"].as<string>();

        if (result.count("i"))
            iframesmpd_o = result["i"].as<string>().c_str();

        if (result.count("d"))
            drmconf_o = result["d"].as<string>().c_str();

        if (result.count("a"))
            alternativeconf_o = true;
    }
    catch (const cxxopts::OptionException& e)
    {
        cout << "error parsing options: " << e.what() << endl;
        exit(1);
    }

    const char* inmpd = inmpd_o.length() ? inmpd_o.c_str() : NULL;
    const char* drmconf = drmconf_o.length() ? drmconf_o.c_str() : NULL;
    const char* iframesmpd = iframesmpd_o.length() ? iframesmpd_o.c_str() : NULL;
    const bool alternativeconf = alternativeconf_o;

    morph_process(inmpd, drmconf, iframesmpd, alternativeconf);

    return EXIT_SUCCESS;
}

#ifdef __cplusplus
}
#endif


