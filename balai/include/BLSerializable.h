#ifndef BL_SERIALIZABLE_H
#define BL_SERIALIZABLE_H

#include "BLCore.h"

namespace mlabs { namespace balai { namespace fileio {

struct XMLAttrib;

} } }

namespace mlabs { namespace balai {

class ISerializable
{
public:
	virtual bool SerializeAttributes(fileio::XMLAttrib const *attribs, uint32 nAttribs) = 0;
};

} }

#endif // BL_SERIALIZABLE_H