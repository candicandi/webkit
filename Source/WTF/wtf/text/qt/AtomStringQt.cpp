#include "config.h"

#include <QString>
#include <wtf/text/WTFString.h>

namespace WTF {

AtomString::AtomString(const QString& qstr)
    : m_string(AtomStringImpl::add({ reinterpret_cast_ptr<const UChar*>(qstr.constData()), static_cast<std::size_t>(qstr.length()) }))
{
    if (qstr.isNull())
        return;
}

}
