/**
 * 用默认 gcc + newlib 链接时：main 返回后由 crt 调 exit → ECALL(93)。
 * 若模拟器曾给错 argv（a1 指向 argc），newlib 会在 .bss 清零等处异常循环。
 *
 * 裸机：./compile.sh c tests/your_test.c（见 tools/interrupt/README.md）
 */
int main(void) {
    int a = 5;
    int b = 3;
    int c = a + b;
    return c;
}
