/* Link-only dependency probe, never a gameplay acceptance test. */
extern void bhEne07(void*);
int main(int argc, char** argv)
{
    if (argc == 123456) bhEne07(argv);
    return 0;
}
