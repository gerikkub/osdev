
static int errno = 0;

int* z_errno(void) {
    return &errno;
}