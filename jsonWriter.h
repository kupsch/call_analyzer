//  Copyright 2022 James A. Kupsch
// 
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
// 
//      http://www.apache.org/licenses/LICENSE-2.0
// 
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.


#include <iostream>
#include <string>
#include <stack>
#include <cstdlib>
#define JSON_WRITER_FATAL_ERR(msg)  do { std::cerr << __FILE__ << ":" << __LINE__ << " JsonWriter Fatal Error: " << msg << std::endl; abort(); } while (0)

class JsonWriter
{
    public:
	JsonWriter(std::ostream& outStream = std::cout, int indentSpaces = 2, int initialLevel = 0);
	JsonWriter(int indentSpaces, int initialLevel = 0)
	    : JsonWriter(std::cout, indentSpaces, initialLevel)
	    {}
	void AddScalar(double d);
	void AddScalar(int i);
	void AddScalar(unsigned int u);
	void AddScalar(long i);
	void AddScalar(unsigned long u);
	void AddScalar(long long i);
	void AddScalar(unsigned long long u);
	void AddScalar(std::string s);
	void AddScalar(const char *s);
	void AddScalar(bool b);
	void AddNull();
	void OpenArray();
	void CloseArray();
	void OpenObject();
	void CloseObject();
	void AddMemberKey(std::string s);
	void End();
	void Reset();
    private:
	enum ItemType {noType, anyType, arrayElemType, objectMemberType};
	enum ItemSpeciality {itemOrdinary, itemClosing, itemKey};
	struct ItemState
	{
	    ItemState(ItemType t): type(t) {}
	    ItemType	type;
	    int		numElements = 0;
	    int		level = 0;
	};

	ItemState&	CurItem();
	void		PushItem(ItemType t);
	void		PopItem(ItemType t);
	void		IncElements();
	int		NumElements();
	void		RequiresAllowsAnyType();
	std::string	JsonString(std::string s);
	void		WritePreitemPunctuation(ItemSpeciality speciality = itemOrdinary);
	void		WriteDelim(char delim, ItemSpeciality speciality = itemOrdinary);
	void		OpenItem(ItemType type, char delim);
	void		CloseItem(ItemType type, char delim);

	std::ostream&		os;
	int			indent;
	std::stack<ItemState>	state;
};


JsonWriter::JsonWriter(std::ostream& outStream, int indentSpaces, int initialLevel)
    :
	os(outStream),
	indent(indentSpaces)
{
    PushItem(anyType);
    CurItem().level = initialLevel;
}


void JsonWriter::AddScalar(double d)
{
    WritePreitemPunctuation();
    os << d;
}


void JsonWriter::AddScalar(int i)
{
    WritePreitemPunctuation();
    os << i;
}


void JsonWriter::AddScalar(unsigned int u)
{
    WritePreitemPunctuation();
    os << u;
}


void JsonWriter::AddScalar(long i)
{
    WritePreitemPunctuation();
    os << i;
}


void JsonWriter::AddScalar(unsigned long u)
{
    WritePreitemPunctuation();
    os << u;
}


void JsonWriter::AddScalar(long long i)
{
    WritePreitemPunctuation();
    os << i;
}


void JsonWriter::AddScalar(unsigned long long u)
{
    WritePreitemPunctuation();
    os << u;
}


void JsonWriter::AddScalar(std::string s)
{
    WritePreitemPunctuation();
    os << JsonString(s);
}


void JsonWriter::AddScalar(const char *s)
{
    WritePreitemPunctuation();
    os << JsonString(s);
}


void JsonWriter::AddScalar(bool b)
{
    WritePreitemPunctuation();
    os << (b ? "true" : "false");
}


void JsonWriter::AddNull()
{
    WritePreitemPunctuation();
    os << "null";
}


void JsonWriter::OpenArray()
{
    OpenItem(arrayElemType, '[');
}


void JsonWriter::CloseArray()
{
    CloseItem(arrayElemType, ']');
}


void JsonWriter::OpenObject()
{
    OpenItem(objectMemberType, '{');
}


void JsonWriter::CloseObject()
{
    if (NumElements() % 2 == 1)  {
	JSON_WRITER_FATAL_ERR("Expected Value before CloseObject");
    }
    CloseItem(objectMemberType, '}');
}


void JsonWriter::AddMemberKey(std::string s)
{
    WritePreitemPunctuation(itemKey);
    os << JsonString(s) << ':';
}


void JsonWriter::End()
{
    if (indent > 0)  {
	os << '\n';
    }

    auto stackSize = state.size();
    if (stackSize > 1)  {
	JSON_WRITER_FATAL_ERR("missing close arrays or objects: " << stackSize - 1);
    }
    if (stackSize != 1)  {
	JSON_WRITER_FATAL_ERR("invalid stack" << ": " << stackSize);
    }

    auto ItemType = CurItem().type;
    if (ItemType != anyType)  {
	JSON_WRITER_FATAL_ERR("invalid top of stack type: " << ItemType);
    }

    if (NumElements() == 0)  {
	JSON_WRITER_FATAL_ERR("No object written");
    }
}


void JsonWriter::Reset()
{
    while (state.size() > 0)  {
	state.pop();
    }
    state.push(anyType);
}


JsonWriter::ItemState& JsonWriter::CurItem()
{
    return state.top();
}


void JsonWriter::PushItem(ItemType t)
{
    int level = 0;
    if (state.size())  {
	level = CurItem().level + 1;
    }
    state.push(t);
    CurItem().level = level;
}


void JsonWriter::PopItem(ItemType t)
{
    auto ItemType = CurItem().type;
    if (t != ItemType)  {
	JSON_WRITER_FATAL_ERR("PopItem Mismatched types have " << ItemType << " need " << t);
    }

    if (state.size() > 1)  {
	state.pop();
    }
}


void JsonWriter::IncElements()
{
    ++CurItem().numElements;
}


int JsonWriter::NumElements()
{
    return CurItem().numElements;
}


void JsonWriter::RequiresAllowsAnyType()
{
    auto type = CurItem().type;
    auto numElements = NumElements();

    if (type == anyType && numElements != 0)  {
	JSON_WRITER_FATAL_ERR("Only 1 top-level value allowed");
    }

    if (type == objectMemberType && numElements % 2 == 0)  {
	JSON_WRITER_FATAL_ERR("Expected AddMemberKey");
    }
}


std::string JsonWriter::JsonString(std::string s)
{
    std::string out{"\""};

    for (auto c: s)  {
	if (c == '\n')  {
	    out += "\\n";
	}  else  {
	    if (c == '\\' || c == '"')  {
		out += '\\';
	    }
	    out += c;
	}
    }
    out += "\"";

    return out;
}


void JsonWriter::WritePreitemPunctuation(ItemSpeciality speciality)
{
    auto type = CurItem().type;
    auto numElements = NumElements();
    bool isClosing = (speciality == itemClosing);
    if (speciality != itemClosing)  {
	if (speciality == itemOrdinary)  {
	    RequiresAllowsAnyType();
	}
	IncElements();
    }
    if (type == arrayElemType || type == objectMemberType)  {
	if (numElements == 0 && isClosing)  {
	    return;
	}

	if (type == objectMemberType && numElements % 2 == 1)  {
	    if (indent != 0)  {
		os << ' ';
	    }
	    return;
	}

	if (numElements > 0 && !isClosing)  {
	    os << ',';
	}
    }

    if (indent != 0 && type != anyType)  {
	os << '\n';;
    }

    auto level = CurItem().level;
    if (isClosing && level > 0)  {
	--level;
    }
    os << std::string(level * indent, ' ');
}


void JsonWriter::WriteDelim(char delim, ItemSpeciality speciality)
{
    WritePreitemPunctuation(speciality);
    os << delim;
}


void JsonWriter::OpenItem(ItemType type, char delim)
{
    WriteDelim(delim);
    PushItem(type);
}


void JsonWriter::CloseItem(ItemType type, char delim)
{
    WriteDelim(delim, itemClosing);
    PopItem(type);
}
