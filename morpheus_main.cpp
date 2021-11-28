#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

void morph_process(const char* td);

int main (void) {

    const char* inmpd = "bbb.mpd";
    morph_process(inmpd);

    return 0;
}

#ifdef __cplusplus
}
#endif


