#pragma once

#include <vector>
#include <string>
#include <map>
#include <iostream>

namespace LiteGraph {

	extern bool verbose;

	struct vec2 { float x; float y;	};
	struct vec3 { float x; float y; float z; };
	struct vec4 { float x; float y; float z; float w; };
	typedef vec4 quat;
	struct mat3 { float m[9]; };
	struct mat4 { float m[16]; };

	class LData;
	class LEvent;
	class LSlot;
	class LLink;
	class LGraph;
	class LGraphNode;

	typedef void* JSON;

	static std::map<std::string, LGraphNode*> node_types;
	void registerNodeType(LGraphNode* node);
	LGraphNode* createNode(const char* name);
	
	enum DataType {
		NONE,
		CUSTOM,
		ENUM,
		NUMBER,
		STRING,
		BOOL,
		VEC2,
		VEC3,
		VEC4,
		QUAT,
		MAT3,
		MAT4,
		ARRAY,		//array of LData, used to store an array of generic info
		OBJECT,		//generic object that must be stored inside WARNING: CANNOT CONTAIN POINTERS or VIRTUAL METHODS
		POINTER,	//pointer to something outside, it only stores the address
		EVENT,		//LEvent
		JSON_OBJECT,//TODO
		ANY
	};

	static std::string datatypes[] = { "NONE", "CUSTOM", "ENUM", "STRING", "BOOL", "VEC2", "VEC3", "VEC4", "QUAT", "MAT3", "MAT4", "ARRAY", "OBJECT", "POINTER", "EVENT", "JSON_OBJECT", "ANY" };
	
	static DataType dataToType(const bool& v) { return DataType::BOOL; }
	static DataType dataToType(const float& v) { return DataType::NUMBER; }
	static DataType dataToType(const double& v) { return DataType::NUMBER; }
	static DataType dataToType(const int& v) { return DataType::NUMBER; }
	static DataType dataToType(const long& v) { return DataType::NUMBER; }
	static DataType dataToType(const vec2& v) { return DataType::VEC2; }
	static DataType dataToType(const vec3& v) { return DataType::VEC3; }
	static DataType dataToType(const vec4& v) { return DataType::VEC4; }
	static DataType dataToType(const mat3& v) { return DataType::MAT3; }
	static DataType dataToType(const mat4& v) { return DataType::MAT4; }
	static DataType dataToType(const LEvent& v) { return DataType::EVENT; }
	static DataType dataToType(const char* v) { return DataType::STRING; }
	static DataType dataToType(const std::string&) { return DataType::STRING; }
	static DataType dataToType(const std::vector<LData*>& v) { return DataType::ARRAY; }
	template<class T> static DataType dataToType(const T& v) { return DataType::OBJECT; }
	template<class T> static DataType dataToType(const T* v) { return DataType::POINTER; }

	#define LEVENT_SIZE 255

	//used when triggering events
	class LEvent {
	public:
		char type[LEVENT_SIZE];
		char data[LEVENT_SIZE];
		float num; //may be useful
		LEvent() { type[0] = 0; data[0] = 0; num = 0; }
		LEvent(const char* type) { setType(type);  data[0] = 0; num = 0; }
		LEvent(const char* type, const char* data) { setType(type); setData(data); num = 0; }
		void setType(const char* type) { strcpy_s(this->type, LEVENT_SIZE, type); }
		void setData(const char* data) { strcpy_s(this->data, LEVENT_SIZE, data); }
		void operator = (const LEvent& e) { setType(e.type); setData(e.data); num = e.num; }
	};

	void registerCustomDataType(const char* name, int id);
	DataType stringToType(const char* str);

	//total, 32 bytes per data (variable in case of dynamic content)
	class LData {
	public:
		DataType type;

		//generic types (this ones cannot have pointers inside and must be lightweight)
		union {
			bool boolean;
			uint8_t enumeration;
			double number;
			vec2 vector2;
			vec3 vector3;
			vec4 vector4;
			quat quaternion;
			void* pointer; //to store generic stuff
		};

		//used to store large objects or objects with variable size
		int bytes; //allocated bytes
		void* custom_data; //generic pointer

		LData();
		~LData();
		void clear();
		void setType(DataType type);

		void assign(int v) { setType(DataType::NUMBER); number = v; }
		void assign(float v) { setType(DataType::NUMBER); number = v; }
		void assign(double v) { setType(DataType::NUMBER); number = v; }
		void assign(vec2 v) { setType(DataType::VEC2); vector2 = v; }
		void assign(vec3 v) { setType(DataType::VEC3); vector3 = v; }
		void assign(vec4 v) { setType(DataType::VEC4); vector4 = v; }
		//void assign(quat v) { setType(DataType::QUAT); quaternion = v; } //same as vec4 so no need
		void assign(void* pointer) { setType(DataType::POINTER); this->pointer = pointer; }

		void assign(mat3 v);
		void assign(mat4 v);
		void assign(const LEvent& v);
		void assign(const char* str);
		void assign(const std::string& str);
		void assign(void* pointer, int size);
		void assign(const std::vector<LData*>& v);
		template<class T> void assignObject(const T& obj);

		LEvent getEvent();
		std::string getString();
		std::vector<LData*> getArray();
		void* const getPointer(); //for safe accessing the data
		void* getObject();
		template<class T> T getObject(const T& v) //Used with custom types
		{
			T obj;
			if (type == DataType::OBJECT && sizeof(T) == bytes)
				memcpy(&obj, custom_data, bytes);
			return obj;
		}
		
		void operator = (const LData& v);
		void operator = (const bool& v) { assign(v); }
		void operator = (const float& v) { assign(v); }
		void operator = (const std::string & v) { assign(v); }
		void operator = (const LEvent& v) { assign(v); }
	};

	class LLink {
	public:
		int id;
		int origin_id;
		int origin_slot;
		int target_id;
		int target_slot;

		LLink(int id, int origin_id, int origin_slot, int target_id, int target_slot);
	};

	class LSlot {
	public:
		std::string name;	//slot name can beused by some nodes during execution to define data to send (specially in variable slots nodes)
		DataType type;		//expected data type for this slot, could be DataType::ANY if several are supported
		int custom_type;	//used by some nodes to precompute comparisons based on slot name (to optimize execution)
		LData* data;		//pointer to the slot data
		LGraphNode* node;	//parent node

		LLink* link;		//for input slots (one single connection allowed)
		std::vector<LLink*> links; //for output slots (multiple connections allowed)

		LSlot(LGraphNode* node, const char* name, DataType type)
		{
			this->type = type;
			this->name = name;
			data = NULL;
			link = NULL;
			this->node = node;
			custom_type = -1;
		}

		~LSlot();

		bool isConnected() { return link != NULL || links.size(); }
		LData* getOriginData();
	};

	#define REGISTERNODE(NODE_NAME,NODE_CLASS) \
		LiteGraph::LGraphNode* clone() { return new NODE_CLASS(); } \
		const char* getType() { return NODE_NAME; } \
		bool mustRegister() { static bool must = true; if(!must) return false; must = false; return true; }


	#define CTOR_NODE() 	if (mustRegister())	LiteGraph::registerNodeType(this);

	class LGraphNode {
	public:
		int id;
		int flags;

		LGraph* graph;
		int order;

		vec2 position;
		vec2 size;

		std::vector<LSlot*> inputs;
		std::vector<LSlot*> outputs;

		void* custom_data;

		LGraphNode();
		virtual ~LGraphNode();
		virtual void onExecute() {};

		LSlot* addInput(const char* name, DataType type);
		LSlot* addOutput(const char* name, DataType type);

		LSlot* getInputSlot(int i);
		LSlot* getOutputSlot(int i);

		int findInputSlotIndex(std::string& name);
		int findOutputSlotIndex(std::string& name);

		LGraphNode* getInputNode(int slot_index);
		std::vector<LGraphNode*> getOutputNodes(int slot_index);

		bool isInputConnected(int index);
		bool isOutputConnected(int index);

		void connect(int output_slot, LGraphNode* target_node, int target_input_slot);
		void disconnectInput(int input_slot);
		void disconnectOutput(int output_slot);

		LData* getInputData(int slot);
		bool getInputDataAsBoolean(int slot);
		double getInputDataAsNumber(int slot);
		std::string getInputDataAsString(int slot);
		JSON getInputDataAsJSON(int slot);

		//allows to send data directly without specifying the type manually
		template<typename T> void setOutputData(int index, const T& v) {
			LSlot* slot = getOutputSlot(index);
			if (!slot)
				return;
			LData* data = slot->data;
			if (data == NULL || (slot->type != dataToType(v) && slot->type != DataType::ANY))
			{
				std::cout << "output data dont match: " << slot->type << " expected " << datatypes[dataToType(v)] << std::endl;
				return;
			}
			data->assign(v);
		}

		void setOutputDataAsBoolean(int slot, const bool& v);
		void setOutputDataAsNumber(int slot, const int& v);
		void setOutputDataAsNumber(int slot, const float& v);
		void setOutputDataAsNumber(int slot, const double& v);
		void setOutputDataAsString(int slot, const char* v);
		void setOutputDataAsPointer(int slot, void* v);
		void setOutputDataAsEvent(int slot, const LEvent& event);

		void trigger(int slot, const LEvent& event);
		virtual void onAction(int slot, const LEvent& event) {};

		virtual const char* getType() { return ""; }
		virtual LGraphNode* clone() { return new LGraphNode();  }; //must use REGISTERNODE( type, classname )
		virtual bool mustRegister() { return false; }

		virtual void configure(void* json_object);
		virtual void serialize(); //not very necessary right now

		virtual void onConfigure(void* json) {}

		void removeSlots();
	};

	class LGraph {
	public:

		std::vector<LGraphNode*> nodes;
		std::map<int,LGraphNode*> nodes_by_id;
		std::vector<LLink*> links;
		std::map<int, LLink*> links_by_id;

		std::vector<LGraphNode*> nodes_in_execution_order;

		std::map<std::string, LData*> outputs;

		int id; //could be helpful, not used for anything
		int last_node_id;
		int last_link_id;

		bool has_errors;

		double time; //in seconds

		void* custom_data;

		LGraph();
		virtual ~LGraph();
		void clear();

		void add(LGraphNode* node);
		LGraphNode* getNodeById(int id);

		void runStep(float dt = 0);

		virtual bool configure( std::string data );

		void sortByExecutionOrder();

		void setOutput(std::string name, LData* data);
	};

	std::string getFileContent(const std::string& path);

	//wrapper for the JSON parser
	bool readJSONBoolean(JSON obj, const char* name, bool& dst);
	bool readJSONNumber(JSON obj, const char* name, int& dst);
	bool readJSONNumber(JSON obj, const char* name, float& dst);
	bool readJSONNumber(JSON obj, const char* name, double& dst);
	bool readJSONString(JSON obj, const char* name, std::string& dst);
	JSON getJSONObject(JSON obj, const char* name);
	JSON parseJSON(const char* str);
	void freeJSON(JSON obj);
}