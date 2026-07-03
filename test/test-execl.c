#include <unistd.h>
int main() {
        return execl("/usr/bin/env", "/usr/bin/env", "A=A", "B=B", "C=C", NULL);
}
