#ifndef hpp_StaticAssert_hpp
#define hpp_StaticAssert_hpp


#define CompileTimeAssert(X)         ((void)sizeof(char[1 - 2*(!(X))]))
#define CompileTimeAssertFalse(Type) do { Type t; char a[1 - sizeof(Type[2]) / sizeof(t)]; } while (0)


#endif
