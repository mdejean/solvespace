#include <si.h>
#include <siapp.h>

//This will result in a linker error if the compiler or architecture don't match

int main(void) {
    SiInitialize();
    SiTerminate();
    return 0;
}