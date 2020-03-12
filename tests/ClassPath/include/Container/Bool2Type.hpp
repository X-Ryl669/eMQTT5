#ifndef hpp_Bool2Type_hpp
#define hpp_Bool2Type_hpp

// Need type declaration too (for Bool2Type declaration)
#ifdef __cplusplus
/** Some helper structure to prevent Visual C++ 6 compiler bug */
template <bool> struct Bool2Type{};

#ifdef _MSC_VER
    // Identifier truncated to 255 char in debug info
    #pragma warning(disable:4786)
#endif
#endif



#endif
