#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <cheri/cheric.h>

int main(void)
{
    printf("Hello World!\n");
    void *sealing_root;
    size_t size = sizeof(sealing_root);
    if (sysctlbyname("security.cheri.sealcap", &sealing_root, &size, NULL, 0) < 0) {
        puts("Error getting root sealing cap");
        return 1;
    }
    if ((cheri_getperm(sealing_root) & CHERI_PERM_SEAL) == 0){
        puts("Got a cap without sealing bit");
        return 1;
    }
    if (cheri_getlen(sealing_root) == 0){
        puts("Got a zero length sealing cap");
        return 1;
    }
    printf("%lu %lu %lu", cheri_getlen(sealing_root), cheri_getbase(sealing_root), cheri_getoffset(sealing_root));
    
    int *p = (int *) malloc(sizeof(int)*10);
    p[0] = 10;
    puts("first write");
    free(p);

    p = cheri_seal(p, sealing_root);
    p = cheri_unseal(p, sealing_root);
    p[0] = 11;
    return 0;
}
