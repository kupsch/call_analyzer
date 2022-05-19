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
#include <fstream>
#include <string>
#include <vector>
#include "Symtab.h"
#include "CodeObject.h"
#include "Instruction.h"
#include "CFG.h"
#include "ABI.h"
#include "Function.h"
#include "jsonWriter.h"



class FunctionSummary;

using BlockAddress = unsigned long;
using Block = Dyninst::ParseAPI::Block;
using Instruction = Dyninst::InstructionAPI::Instruction;
using RegisterAST = Dyninst::InstructionAPI::RegisterAST;
using RegisterAST = Dyninst::InstructionAPI::RegisterAST;
using RegisterSet = std::set<RegisterAST::Ptr>;
using AddressVector = std::vector<BlockAddress>;
using BlockAddressSet = std::set<BlockAddress>;

class BlockSummary
{
    public:
	BlockSummary(FunctionSummary *f, Block *b);
	void AddParamReg(MachRegister r);
	BlockAddress Addr() const
	{
	    return block->start();
	}
	bool IsCallBlock() const;
	void IsCallBlock(bool b);
	bool IsSysCallBlock() const;
	void IsSysCallBlock(bool b);

	void SetStartRegs(bitArray regs);
	bitArray StartRegs() const;
	bitArray UsedRegs() const;
	bitArray OutRegs() const;
	bitArray CallSiteRegs() const;
	bitArray EnptyRegs() const;

	AddressVector Predecessors() const;
	AddressVector Successors() const;

	std::vector<std::string> CallNames() const;
	void WriteJson(JsonWriter &writer) const;
	void WriteJsonCall(
		JsonWriter &writer,
		Address callAddr,
		const std::vector<std::string> liveRegs,
		std::vector<std::string> callNames,
		bool isToPlt
		) const;

    private:
	void SummarizeBlock();
	void SummarizeInstruction(Instruction i);
	ABI *abi() const;
	int AbiRegisterId(const RegisterAST::Ptr &r) const;
	int PromotedRegisterId(const RegisterAST::Ptr &r) const;
	bitArray RegisterSetToBitmap(const RegisterSet &rs) const;
	Architecture Arch() const;
	
	FunctionSummary	*function;
	Block		*block;
	bitArray	startRegs;
	bitArray	usedRegs;
	Address		callInsnAddr = 0;
	bool		isCallBlock = false;
	bool		isSysCallBlock = false;
};

bool operator==(const BlockSummary &a, const BlockSummary &b)
{
    return a.Addr() == b.Addr();
}

bool operator<(const BlockSummary &a, const BlockSummary &b)
{
    return a.Addr() < b.Addr();
}

using BlockSummarySet = std::set<BlockSummary>;
using BlockSummaryMap = std::map<BlockAddress, BlockSummary>;



class FunctionSummary
{
    public:
	using Function = Dyninst::ParseAPI::Function;

	FunctionSummary(Function *f);

	ABI *abi()
	{
	    return theAbi;
	}

	void AddParamRegs();
	BlockSummary *AddBlock(Block *b);
	BlockSummary *GetBlock(BlockAddress a);
	const BlockSummary *GetBlock(BlockAddress a) const;
	std::string RegIdToName(int id) const;
	std::vector<std::string> RegBitmapToNames(bitArray regs) const;
	Dyninst::SymtabAPI::Symtab *SymtabObject() const;
	std::string RegionName() const;
	static std::string RegionName(Function *func);
	bool IsPltRegion() const;
	static bool IsPltRegion(Function *func);
	void PropagateStartRegs();
	bitArray CallParamRegisters() const
	{
	    return callParamRegisters;
	}
	bitArray CallReturnRegisters() const
	{
	    return callReturnRegisters;
	}
	bitArray CallNotKilledRegisters() const
	{
	    return callNotKilledRegisters;
	}
	std::string FunctionName() const;
	Address FunctionStartAddr() const;
	static void WriteJsonAddressMember(JsonWriter &writer, std::string name, Address a);
	void WriteJson(JsonWriter &writer) const;
    private:
	Function 				*function;
	ABI					*theAbi;
	BlockSummaryMap 			blocks;
	BlockAddressSet				callBlocks;
	static bool				initializedStatics;
	static std::map<int, MachRegister>	regIdToReg;
	static bitArray				callParamRegisters;
	static bitArray				callReturnRegisters;
	static bitArray				callNotKilledRegisters;
};


bool FunctionSummary::initializedStatics = false;
std::map<int, MachRegister> FunctionSummary::regIdToReg;
bitArray FunctionSummary::callParamRegisters;
bitArray FunctionSummary::callReturnRegisters;
bitArray FunctionSummary::callNotKilledRegisters;


char emptyString[] = "";
struct Options
{
    void ProcessOptions(int argc, char **argv);
    void Error(const std::string &msg);
    bool			help = false;
    bool			version = false;
    bool			debug = false;
    bool			onlyToPltCalls = true;
    int				indent = 2;
    bool			failed = false;
    std::string			failureMsg;
    std::vector<char*>	args;
    std::string			programVersion{"0.9.0"};
    char *			inputFile = nullptr;
    char *			outputFile = nullptr;
    char *			programName = emptyString;
};

Options options;




Architecture BlockSummary::Arch() const
{
    return block->obj()->cs()->getArch();
}


inline BlockSummary::BlockSummary(FunctionSummary *f, Block *b) :
    function(f),
    block(b),
    startRegs(EnptyRegs()),
    usedRegs(EnptyRegs())
{
    using namespace std;
    SummarizeBlock();
}


void BlockSummary::SummarizeBlock()
{
    using namespace InstructionAPI;

    Block::Insns instructions;
    block->getInsns(instructions);
    for (auto i: instructions)  {
	SummarizeInstruction(i.second);
	switch (i.second.getCategory())  {
	    case c_CallInsn:
		callInsnAddr = i.first;
		IsCallBlock(true);
		break;
	    case c_SysEnterInsn:
	    case c_SyscallInsn:
		IsSysCallBlock(true);
		break;
	    default:
		// ordinary instruction
		break;
	}
    }
}


void BlockSummary::AddParamReg(MachRegister r)
{
    using namespace InstructionAPI;

    auto regAST = new RegisterAST{r};
    auto reg = RegisterAST::Ptr{regAST};
    auto regId = PromotedRegisterId(reg);
    usedRegs[regId] = 1;
}


bool BlockSummary::IsCallBlock() const
{
    return isCallBlock;
}


void BlockSummary::IsCallBlock(bool b)
{
    isCallBlock = b;
}


bool BlockSummary::IsSysCallBlock() const
{
    return isSysCallBlock;
}


void BlockSummary::SetStartRegs(bitArray regs)
{
    startRegs = regs;
}


bitArray BlockSummary::StartRegs() const
{
    return startRegs;
}


bitArray BlockSummary::UsedRegs() const
{
    return usedRegs;
}


bitArray BlockSummary::OutRegs() const
{
    bitArray out{usedRegs};
    out |= startRegs;
    if (IsCallBlock())  {
	out &= function->CallNotKilledRegisters();
	out |= function->CallReturnRegisters();
    }

    return out;
}


bitArray BlockSummary::CallSiteRegs() const
{
    bitArray out{usedRegs};
    out |= startRegs;

    return out;
}


AddressVector BlockSummary::Predecessors() const
{
    AddressVector addrs;
    for (auto e: block->sources())  {
	if (!e->interproc())  {
	    auto blockAddr = e->src()->start();
	    if (blockAddr != BlockAddress(-1))  {
		addrs.push_back(blockAddr);
	    }
	}
    }

    return addrs;
}


AddressVector BlockSummary::Successors() const
{
    AddressVector addrs;
    for (auto e: block->targets())  {
	if (!e->interproc())  {
	    auto blockAddr = e->trg()->start();
	    if (blockAddr != BlockAddress(-1))  {
		addrs.push_back(blockAddr);
	    }
	}
    }

    return addrs;
}


std::vector<std::string> BlockSummary::CallNames() const
{
    std::vector<std::string> names;

    std::vector<ParseAPI::Function *> funcs;
    auto i = back_inserter(funcs);
    block->getFuncs(i);

    for (auto func: funcs)  {
	names.push_back(func->name());
    }

    return names;
}


void BlockSummary::WriteJsonCall(
	JsonWriter &writer,
	Address callAddr,
	const std::vector<std::string> liveRegs,
	std::vector<std::string> callNames,
	bool isToPlt
    ) const
{
    if (options.onlyToPltCalls && !isToPlt)  {
	return;
    }

    writer.OpenObject();

    function->WriteJsonAddressMember(writer, "callInstructionAddr", callInsnAddr);
    function->WriteJsonAddressMember(writer, "calledAddr", callAddr);
    writer.AddMemberKey("callToPlt");
    writer.AddScalar(isToPlt);
    writer.AddMemberKey("liveRegisters");
    writer.OpenArray();
    for (auto name : liveRegs)  {
	writer.AddScalar(name);
    }
    writer.CloseArray();
    writer.AddMemberKey("funcNames");
    writer.OpenArray();
    for (auto name: callNames)  {
	writer.AddScalar(name);
    }
    writer.CloseArray();
    writer.CloseObject();
}


void BlockSummary::WriteJson(JsonWriter &writer) const
{
    using namespace std;

    auto usedRegs = UsedRegs() & function->CallParamRegisters();
    auto regNames = function->RegBitmapToNames(usedRegs);

    int numCallTargets = 0;
    for (auto e : block->targets())  {
	auto outBlock = e->trg();
	auto callAddr = outBlock->start();
	if (e->type() == ParseAPI::CALL)  {
	    bool isToPlt = false;
	    vector<ParseAPI::Function *> funcs;
	    auto i = back_inserter(funcs);
	    outBlock->getFuncs(i);
	    vector<string> funcNames;
	    for (auto f: funcs)  {
		isToPlt |= function->IsPltRegion(f);
		funcNames.push_back(f->name());
	    }
	    ++numCallTargets;
	    WriteJsonCall(writer, callAddr, regNames, funcNames, isToPlt);
	}
    }
    
    if (numCallTargets == 0)  {
	WriteJsonCall(writer, Address(-1), regNames, {}, false);
    }
}
    

void BlockSummary::IsSysCallBlock(bool b)
{
    isSysCallBlock = b;
}


void BlockSummary::SummarizeInstruction(Instruction i)
{
    RegisterSet regs;
    i.getReadSet(regs);
    i.getWriteSet(regs);
    usedRegs |= RegisterSetToBitmap(regs);
}


ABI *BlockSummary::abi() const
{
    return function->abi();
}


bitArray BlockSummary::EnptyRegs() const
{
    return abi()->getBitArray();
}


int BlockSummary::AbiRegisterId(const RegisterAST::Ptr &r) const
{
    return abi()->getIndex(r->getID());
}


int BlockSummary::PromotedRegisterId(const RegisterAST::Ptr &r) const
{
    auto promotedReg = r->promote(r);
    auto id = AbiRegisterId(promotedReg);
    if (id != -1)  {
	return id;
    }  else  {
	return AbiRegisterId(r);
    }
}


bitArray BlockSummary::RegisterSetToBitmap(const RegisterSet &rs) const
{
    auto bitmap{EnptyRegs()};
    for (auto r: rs)  {
	auto regId = PromotedRegisterId(r);
	bitmap[regId] = 1;
    }

    return bitmap;
}




FunctionSummary::FunctionSummary(Function *f) :
    function(f),
    theAbi(ABI::getABI(f->obj()->cs()->getAddressWidth()))
{
    using namespace std;

    if (!initializedStatics)  {
	for (auto i: *abi()->getIndexMap())  {
	    regIdToReg[i.second] = i.first;
	}

	// param registers: rax rcx rsi rdi r8 r9 xxm0-7
	callParamRegisters = theAbi->getCallReadRegisters();

	// return registers: rax rdx xxm0-1
	callReturnRegisters = theAbi->getReturnRegisters();
	callReturnRegisters[109] = 1;	// +xxm0
	callReturnRegisters[110] = 1;	// +xxm1

	// caller saved registers: rbx rsp rbp r12 r13 r14 r15
	// not killed registers: callerSavedRegs | returnRegs
	// not killed registers: rax rdx rbx rsp rbp r12 r13 r14 r15 xxm0-1
	callNotKilledRegisters = theAbi->getReturnReadRegisters();
	callNotKilledRegisters[1] = 0;	// -rcx
	callNotKilledRegisters[4] = 1;	// +rsp
	callNotKilledRegisters[5] = 1;	// +rbp

	initializedStatics = true;
    }

    for (auto b: f->blocks())  {
	auto blockSummary = AddBlock(b);
	if (blockSummary->IsCallBlock())  {
	    callBlocks.insert(b->start());
	}
    }

    AddParamRegs();
    PropagateStartRegs();
}


void FunctionSummary::AddParamRegs()
{
    using namespace std;
    using namespace Dyninst;

    if (function->blocks().empty())  {
	return;
    }

    auto symtab = SymtabObject();

    SymtabAPI::Function *symtabFunc;
    auto entryBlock = function->entry();
    Address entryAddr = entryBlock->start();
    bool found = symtab->findFuncByEntryOffset(symtabFunc, entryAddr);
    if (!found)  {
	entryBlock = *function->blocks().begin();
	entryAddr = entryBlock->start();
	found = symtab->findFuncByEntryOffset(symtabFunc, entryAddr);
	if (!found)  {
	    return;
	}
    }
    auto entryBlockLastAddr = entryBlock->end();

    vector <SymtabAPI::localVar*> params;
    symtabFunc->getParams(params);

    for (auto p: params)  {
//jk	cout << "PARAM:  name: " << p->getName() << endl;
	for (auto loc: p->getLocationLists())  {
//jk	    cout << "PARAM:    loc:  \t[" << hex << loc.lowPC
//jk		    << ", " << loc.hiPC << ")"
//jk		    << "  reg:" << loc.mr_reg.name()
//jk		    << "  class:" << loc.stClass
//jk		    << "  ";
	    if (loc.stClass == storageReg || loc.stClass == storageRegOffset)  {
		if (entryBlockLastAddr > loc.lowPC && entryAddr < loc.hiPC)  {
//jk		    cout << "GOOD";
		    if (auto b = GetBlock(entryAddr))  {
			b->AddParamReg(loc.mr_reg);
		    }
		}  else  {
//jk		    cout << "OUT OF RANGE";
		}
	    }  else  {
//jk		cout << "NON-REG CLASS";
	    }
//jk	    cout << endl;
	}
    }
}


BlockSummary *FunctionSummary::AddBlock(Block *b)
{
    auto addr = b->start();
    auto insert_pair = std::make_pair(addr, BlockSummary(this, b));
    auto i = blocks.insert(insert_pair);
    if (!i.second)  {
	std::cerr << "block address (" << addr << ") already processed";
    }

    return &i.first->second;
}


BlockSummary *FunctionSummary::GetBlock(BlockAddress a)
{
    auto i = blocks.find(a);
    if (i != blocks.end())  {
	return &i->second;
    }  else  {
	return nullptr;
    }
}


const BlockSummary *FunctionSummary::GetBlock(BlockAddress a) const
{
    auto i = blocks.find(a);
    if (i != blocks.end())  {
	return &i->second;
    }  else  {
	return nullptr;
    }
}


std::string FunctionSummary::RegIdToName(int id) const
{
    auto name = regIdToReg[id].name();
    auto i = name.rfind(':');
    if (i != name.npos)  {
	name.erase(0, i + 1);
    }
    return name;
}


Dyninst::SymtabAPI::Symtab *FunctionSummary::SymtabObject() const
{
    using namespace Dyninst;
    if (auto o =  dynamic_cast<ParseAPI::SymtabCodeSource *>(function->obj()->cs()))  {
	return o->getSymtabObject();
    }  else  {
	assert("");
	return nullptr;
    }
}


std::vector<std::string> FunctionSummary::RegBitmapToNames(bitArray regs) const
{
    using namespace std;

    vector<string> regNames;
    auto size = regs.size();
    auto i = regs.find_first();
    while (i < size)  {
	regNames.push_back(RegIdToName(i));
	i = regs.find_next(i);
    }

    return regNames;
}


std::string FunctionSummary::RegionName() const
{
    return RegionName(function);
}


std::string FunctionSummary::RegionName(Function *func)
{
    using namespace Dyninst::ParseAPI;

    auto r = func->region();
    if (auto scr = dynamic_cast<SymtabCodeRegion*>(r))  {
	return scr->symRegion()->getRegionName();
    }  else  {
	return "";
    }
}


bool FunctionSummary::IsPltRegion() const
{
    return IsPltRegion(function);
}


bool FunctionSummary::IsPltRegion(Function *func)
{
    using namespace std;

    auto name = RegionName(func);
    return (name.find(".plt") != name.npos);
}


void FunctionSummary::PropagateStartRegs()
{
    using namespace std;

    set<BlockAddress> toProcess;
    for (auto i: blocks)  {
	toProcess.insert(i.first);
    }

    while (!toProcess.empty())  {
	auto i = toProcess.begin();
	auto addr = *i;
	auto block = GetBlock(addr);
	toProcess.erase(i);

//jk	cout << "PropStart:   " << hex << addr << dec << endl;
//jk	if (block)  {
//jk	    for (auto a: block->Predecessors())  {
//jk		cout << "PropStart:     before " << hex << a << dec << endl;
//jk	    }
//jk	    for (auto a: block->Successors())  {
//jk		cout << "PropStart:     after " << hex << a << dec << endl;
//jk	    }
//jk	}

	auto newStartRegs{block->EnptyRegs()};
	for (auto a: block->Predecessors())  {
	    newStartRegs |= GetBlock(a)->OutRegs();
	}
//jk	cout << "PropStart:     old start:" << endl;
//jk	//jk PrintRegs(this, block->StartRegs());
//jk	cout << "PropStart:     new regs:" << endl;
//jk	//jk PrintRegs(this, newStartRegs);

	if (newStartRegs != block->StartRegs())  {
	    block->SetStartRegs(newStartRegs);
	    for (auto a: block->Successors())  {
//jk		cout << "PropStart:     prop  " << hex << a << dec << endl;
		toProcess.insert(a);
	    }
	}
    }
}


std::string FunctionSummary::FunctionName() const
{
    return function->name();
}


Address FunctionSummary::FunctionStartAddr() const
{
    return function->region()->low();
}


void FunctionSummary::WriteJsonAddressMember(JsonWriter &writer, std::string name, Address a)
{
    writer.AddMemberKey(name);
    if (a != Address(-1))  {
	writer.AddScalar(a);
    }  else  {
	writer.AddNull();
    }
}


void FunctionSummary::WriteJson(JsonWriter &writer) const
{
    using namespace std;

    writer.OpenObject();
    writer.AddMemberKey("funcName");
    writer.AddScalar(FunctionName());
    WriteJsonAddressMember(writer, "funcAddr", FunctionStartAddr());
    writer.AddMemberKey("sectionName");
    writer.AddScalar(RegionName());
    writer.AddMemberKey("isInPlt");
    writer.AddScalar(IsPltRegion());
    writer.AddMemberKey("calls");
    writer.OpenArray();

    for (auto b: callBlocks)  {
	auto block = GetBlock(b);
	block->WriteJson(writer);
    }

    writer.CloseArray();
    writer.CloseObject();
}


std::string RegionName(Dyninst::ParseAPI::CodeRegion *r)
{
    using namespace Dyninst::ParseAPI;

    if (auto scr = dynamic_cast<SymtabCodeRegion*>(r))  {
	return scr->symRegion()->getRegionName();
    }  else  {
	return "";
    }
}


std::string RegionTypeName(Dyninst::ParseAPI::CodeRegion *r)
{
    using namespace Dyninst::ParseAPI;

    if (auto scr = dynamic_cast<SymtabCodeRegion*>(r))  {
	auto symRegion = scr->symRegion();
	return symRegion->regionType2Str(symRegion->getRegionType());
    }  else  {
	return "";
    }
}

//using namespace Dyninst;
//using namespace Dyninst::ParseAPI;
using namespace Dyninst::InstructionAPI;

int getAbiRegisterIndex(ABI *abi, RegisterAST::Ptr r)
{
    return abi->getIndex(r->getID());
}


RegisterAST::Ptr promoteRegister(ABI *abi, RegisterAST::Ptr r)
{
    auto rp = r->promote(r);
    if (getAbiRegisterIndex(abi, rp) != -1)  {
	return rp;
    }  else  {
	return r;
    }
}


void Options::ProcessOptions(int argc, char **argv)
{
    using namespace std;

    if (argv[0])  {
	programName = argv[0];
    }

    bool lookingForOptions = true;
    for (int i = 1; i < argc; ++i)  {
	const char *arg = argv[i];
	if (lookingForOptions && arg[0] == '-')  {
	    if (!strcmp("--help", arg) || !strcmp("-h", arg))  {
		help = true;
	    }  else if (!strcmp("--version", arg) || !strcmp("-v", arg))  {
		version = true;
	    }  else if (!strcmp("--debug", arg))  {
		debug = true;
	    }  else if (!strcmp("--", arg))  {
		lookingForOptions = false;
	    }  else if (!strcmp("--compact-json", arg))  {
		indent = 0;
	    }  else if (!strcmp("--all-calls", arg))  {
		onlyToPltCalls = false;
	    }  else  {
		failed = true;
		failureMsg += "Unknown option ";
		failureMsg += string{arg} + '\n';
	    }
	}  else  {
	    lookingForOptions = false;
	    args.push_back(argv[i]);
	}
    }
    
    if (help)  {
	clog << "Usage: " << programName << " [options] infile [outfile]\n"
	    << "  --compact-json   minify json output\n"
	    << "  --all-calls      include all calls to non-external functions\n"
	    << "  --help           print this message and exit\n"
	    << "  --version        print version and exit\n";
	exit(0);
    }

    if (version)  {
	clog << programName << " version " << programVersion << endl;
	exit(0);
    }

    if (args.size() < 1)  {
	failed = true;
	failureMsg += "binary input argument not specified\n";
    }

    if (args.size() > 2)  {
	failed = true;
	failureMsg += "Only two arguments are allowd\n";
    }

    if (failed)  {
	Error(failureMsg);
    }
}


void Options::Error(const std::string &msg)  {
    using namespace std;
    clog << "ERROR: " << programName << "\n" << msg << endl;
    exit(1);
}



int main(int argc, char **argv)
{
    using namespace std;
    using namespace Dyninst;
    using namespace Dyninst::ParseAPI;
    using namespace Dyninst::InstructionAPI;

    options.ProcessOptions(argc, argv);

    if (argc < 2)  {
	return 1;
    }

    auto sts = new ParseAPI::SymtabCodeSource(options.args[0]);
    auto co = new ParseAPI::CodeObject(sts);

    co->parse();

    auto allFuncs = co->funcs();

    std::ostream *jsonFile = &cout;
    std::ofstream outputFile;
    if (options.args.size() > 1)  {
	outputFile.open(options.args[1]);
	if (!outputFile)  {
	    options.Error(string{"Error opening output file '"} + options.args[1] + "'\n");
	}
	jsonFile = &outputFile;
    }

    JsonWriter writer(*jsonFile, options.indent);
    writer.OpenObject();
    writer.AddMemberKey("functions");
    writer.OpenArray();
    for (auto f: allFuncs)  {
	FunctionSummary fsum(f);
	fsum.WriteJson(writer);
    }
    writer.CloseArray();
    writer.CloseObject();
    writer.End();
}
