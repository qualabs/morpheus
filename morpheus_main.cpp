#include <iostream>
#include "cxxopts.hpp"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

void morph_process(const char* encmpd, const char* drmconf, const char* iframesmpd);

int main (int argc, char *argv[]) {

    const char* inmpd = NULL;
    const char* drmconf = NULL;
    const char* iframesmpd = NULL;

    //try
    //{
        cxxopts::Options options(argv[0], "morpheus VOD stand-alone tool");

        options.set_width(80).add_options()
            ("n", "encoder mpd file", cxxopts::value<string>())
            ("i", "iframes track mpd file", cxxopts::value<string>())
            ("d", "ckm encrypt context response xml file", cxxopts::value<string>())
            ("h,help", "Print help")
            ;

        auto result = options.parse(argc, argv);

        if ( result.count("help") || argc == 1 )
        {
            cout << options.help() << endl;
            exit(0);
        }

        if (result.count("n"))
            inmpd = result["n"].as<string>().c_str();

        if (result.count("i"))
            iframesmpd = result["i"].as<string>().c_str();

        if (result.count("d"))
            drmconf = result["d"].as<string>().c_str();
    //}
    /*
    catch (const cxxopts::OptionException& e)
    {
        cout << "error parsing options: " << e.what() << endl;
        exit(1);
    }

    Adding the try catch causes problems, pointers are messed up
    printf("\n");
    printf("\n");
    printf("%s\n", inmpd);
    printf("%s\n", iframesmpd);
    printf("%s\n", drmconf);
    exit(0);
    */

    morph_process(inmpd, drmconf, iframesmpd);

    return EXIT_SUCCESS;
}

#ifdef __cplusplus
}
#endif


