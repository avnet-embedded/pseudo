/* Return vals: 2 - invalid arg list
 *              1 - chroot failed
 *              0 - chroot succeeded
 */
#include <unistd.h>
int main(int argc, char *argv[]) {
        if (argc != 2)
          return 2;
        return (chroot(argv[1]) == -1);
}
