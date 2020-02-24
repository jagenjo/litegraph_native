#pragma once

#include <vector>
#include <string>
#include <map>

namespace LiteGraph {

	extern bool verbose;

	struct vec2 { float x; float y;	};
	struct vec3 { float x; float y; float z; };
	struct vec4 { float x; float y; float z; float w; };
	typedef vec4 quat;
	struct mat3 { float m[9]; };
	struct mat4 { float m[16]; };

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
		ARRAY, //array of LData
		JSON_OBJECT,
		OBJECT, //generic object that must be stored inside
		POINTER,//pointer to something outside
		EVENT,//event
		ANY
	};

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

		//generic types (this ones cannot have pointers inside)
		union {
			bool boolean;
			uint8_t enumeration;
			double number;
			vec2 vector2;
			vec3 vector3;
			vec4 vector4;
			quat quaternion;
			void* pointer;
		};

		//used to store large objects or objects with variable size
		int bytes; //allocated bytes
		void* custom_data; //generic pointer

		LData();
		~LData();
		void clear();
		void setType(DataType type);

		void assign(float v) { setType(DataType::NUMBER); number = v; }
		void assign(vec2 v) { setType(DataType::VEC2); vector2 = v; }
		void assign(vec3 v) { setType(DataType::VEC3); vector3 = v; }
		void assign(vec4 v) { setType(DataType::VEC4); vector4 = v; }
		//void assign(quat v) { setType(DataType::QUAT); quaternion = v; } //same as vec4 so no need
		void assign(void* pointer) { setType(DataType::POINTER); this->pointer = pointer; }

		void assign(mat3 v);
		void assign(mat4 v);
		void assign(const LEvent& v);
		void assign(const char* str);
		void assign(void* pointer, int size);
		void assign(const std::vector<LData*>& v);
		template<class T> void assignObject(const T& obj);

		LEvent getEvent();
		std::string getString();
		std::vector<LData*> getArray();

		void operator = (const LData& v);
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

		std::string name;
		DataType type;
		LData* data; //pointer to the slot data
		LGraphNode* node; //parent node

		LLink* link; //for input slots
		std::vector<LLink*> links; //for output slots

		LSlot(LGraphNode* node, const char* name, DataType type)
		{
			this->type = type;
			this->name = name;
			data = NULL;
			link = NULL;
			this->node = node;
		}

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

		LGraph* graph;
		int order;

		vec2 position;
		vec2 size;

		std::vector<LSlot*> inputs;
		std::vector<LSlot*> outputs;

		LGraphNode();
		virtual void onExecute() {};

		LSlot* addInput(const char* name, DataType type);
		LSlot* addOutput(const char* name, DataType type);

		LSlot* getInputSlot(int i);
		LSlot* getOutputSlot(int i);

		bool isInputConnected(int index);
		bool isOutputConnected(int index);

		//void connect(int output_slot, LGraphNode* target_node, int target_input_slot);

		LData* getInputData(int slot);
		bool getInputDataAsBoolean(int slot);
		double getInputDataAsNumber(int slot);
		std::string getInputDataAsString(int slot);
		JSON getInputDataAsJSON(int slot);

		void setOutputDataAsBoolean(int slot, bool v);
		void setOutputDataAsNumber(int slot, double v);
		void setOutputDataAsString(int slot, const char* v);
		void setOutputDataAsPointer(int slot, void* v);
		void setOutputDataAsEvent(int slot, LEvent event);

		void trigger(int slot, LEvent event);
		virtual void onAction(int slot, LEvent& event) {};

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

		int last_node_id;
		int last_link_id;

		bool has_errors;

		double time; //in seconds

		LGraph();
		void clear();

		void add(LGraphNode* node);
		LGraphNode* getNodeById(int id);

		void runStep(float dt = 0);

		bool configure(std::string data);

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