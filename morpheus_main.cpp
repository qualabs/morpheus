#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

void morph_process(const char* td);

int main (int argc, char *argv[]) {

     if (argc < 2) {
        cout << "Usage: " << argv[0] << " filename.mpd" << endl;
        return EXIT_FAILURE;
    }

    const char *inmpd = argv[1];
    morph_process(inmpd);

    return EXIT_SUCCESS;
}

#ifdef __cplusplus
}
#endif


