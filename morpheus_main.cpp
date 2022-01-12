#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

void morph_process(const char* td, const char* drmconf);

int main (int argc, char *argv[]) {

    if (argc < 2 || argc > 3) {
        cout << "Usage: " << argv[0] << " filename.mpd <drmconfig.xml>" << endl;
        return EXIT_FAILURE;
    }

    bool gotdrm = false;
    if (argc == 3) {
        gotdrm = true;
    }

    const char *inmpd = argv[1];
    const char *drmconf = gotdrm ? argv[2] : NULL;
    morph_process(inmpd, drmconf);

    return EXIT_SUCCESS;
}

#ifdef __cplusplus
}
#endif


