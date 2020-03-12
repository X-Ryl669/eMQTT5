#include "../include/Strings/Strings.hpp"
#include "../include/Hash/Hasher.hpp"
#include "../include/Hash/StringHash.hpp"

namespace Container
{
    template <>
        uint32 HashKey<Strings::FastString>::hashKey(const Strings::FastString & initialKey)
    {
        return Hash::superFastHash(initialKey, initialKey.getLength());
    }

    template <>
        uint32 HashKey<Strings::VerySimpleReadOnlyString>::hashKey(const Strings::VerySimpleReadOnlyString & initialKey)
    {
        return Hash::superFastHash((const char*)initialKey.getData(), initialKey.getLength());
    }

}
