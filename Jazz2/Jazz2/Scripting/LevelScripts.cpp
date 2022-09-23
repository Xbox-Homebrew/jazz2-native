﻿#if defined(WITH_ANGELSCRIPT)

#include "LevelScripts.h"
#include "RegisterArray.h"
#include "RegisterString.h"
#include "ScriptActorWrapper.h"

#include "../LevelHandler.h"
#include "../Actors/ActorBase.h"

#if defined(DEATH_TARGET_WINDOWS) && !defined(CMAKE_BUILD)
#   if defined(_M_X64)
#		if defined(_DEBUG)
#			pragma comment(lib, "../Libs/x64/angelscriptd.lib")
#		else
#			pragma comment(lib, "../Libs/x64/angelscript.lib")
#		endif
#   elif defined(_M_IX86)
#		if defined(_DEBUG)
#			pragma comment(lib, "../Libs/x86/angelscriptd.lib")
#		else
#			pragma comment(lib, "../Libs/x86/angelscript.lib")
#		endif
#   else
#       error Unsupported architecture
#   endif
#endif

#if !defined(DEATH_TARGET_ANDROID) && !defined(_WIN32_WCE) && !defined(__psp2__)
#	include <locale.h>		// setlocale()
#endif

namespace
{
	void asPrint(String& msg)
	{
		LOGI_X("%s", msg.data());
	}

	float asFractionf(float v)
	{
		float intPart;
		return modff(v, &intPart);
	}
}

namespace Jazz2::Scripting
{
	LevelScripts::LevelScripts(LevelHandler* levelHandler, const StringView& scriptPath)
		:
		_levelHandler(levelHandler),
		_module(nullptr),
		_ctx(nullptr)
	{
		_engine = asCreateScriptEngine();

		int r;
		r = _engine->SetMessageCallback(asMETHOD(LevelScripts, Status), this, asCALL_THISCALL); RETURN_ASSERT(r >= 0);

		// Built-in types
		RegisterArray(_engine);
		RegisterString(_engine);
		
		// Math functions
		r = _engine->RegisterGlobalFunction("float cos(float)", asFUNCTIONPR(cosf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float sin(float)", asFUNCTIONPR(sinf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float tan(float)", asFUNCTIONPR(tanf, (float), float), asCALL_CDECL); assert(r >= 0);

		r = _engine->RegisterGlobalFunction("float acos(float)", asFUNCTIONPR(acosf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float asin(float)", asFUNCTIONPR(asinf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float atan(float)", asFUNCTIONPR(atanf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float atan2(float,float)", asFUNCTIONPR(atan2f, (float, float), float), asCALL_CDECL); assert(r >= 0);

		r = _engine->RegisterGlobalFunction("float cosh(float)", asFUNCTIONPR(coshf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float sinh(float)", asFUNCTIONPR(sinhf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float tanh(float)", asFUNCTIONPR(tanhf, (float), float), asCALL_CDECL); assert(r >= 0);

		r = _engine->RegisterGlobalFunction("float log(float)", asFUNCTIONPR(logf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float log10(float)", asFUNCTIONPR(log10f, (float), float), asCALL_CDECL); assert(r >= 0);

		r = _engine->RegisterGlobalFunction("float pow(float, float)", asFUNCTIONPR(powf, (float, float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float sqrt(float)", asFUNCTIONPR(sqrtf, (float), float), asCALL_CDECL); assert(r >= 0);

		r = _engine->RegisterGlobalFunction("float ceil(float)", asFUNCTIONPR(ceilf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float abs(float)", asFUNCTIONPR(fabsf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float floor(float)", asFUNCTIONPR(floorf, (float), float), asCALL_CDECL); assert(r >= 0);
		r = _engine->RegisterGlobalFunction("float fraction(float)", asFUNCTIONPR(asFractionf, (float), float), asCALL_CDECL); assert(r >= 0);

		_module = _engine->GetModule("Main", asGM_ALWAYS_CREATE); RETURN_ASSERT(_module != nullptr);

		// Game-specific functions
		r = _engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(asPrint), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		r = _engine->RegisterGlobalFunction("void RegisterSpawnable(int, const string &in)", asFUNCTION(asRegisterSpawnable), asCALL_CDECL); RETURN_ASSERT(r >= 0);

		ScriptActorWrapper::RegisterFactory(_engine, _module);

		if (!AddScriptFromFile(scriptPath)) {
			LOGE("Cannot compile level script");
			return;
		}

		r = _module->Build(); RETURN_ASSERT_MSG(r >= 0, "Cannot compile the script. Please correct the code and try again.");

		_ctx = _engine->CreateContext();
		_ctx->SetUserData(this, ContextToController);

		asIScriptModule* mod = _engine->GetModule("Main");
		asIScriptFunction* func = mod->GetFunctionByDecl("void main()");
		if (func != nullptr) {
			_ctx->Prepare(func);
			r = _ctx->Execute();
			if (r == asEXECUTION_EXCEPTION) {
				LOGE_X("An exception \"%s\" occurred in \"%s\". Please correct the code and try again.", _ctx->GetExceptionString(), _ctx->GetExceptionFunction()->GetDeclaration());
			}
			//_ctx->Unprepare();
		}
	}

	LevelScripts::~LevelScripts()
	{
		if (_ctx != nullptr) {
			_ctx->Release();
			_ctx = nullptr;
		}
		if (_engine != nullptr) {
			_engine->ShutDownAndRelease();
			_engine = nullptr;
		}
	}

	void LevelScripts::OnBeginLevel()
	{
		// TODO
		auto testActor = CreateActorInstance("TestActor"_s);
		uint8_t params[1];
		params[0] = 1;
		testActor->OnActivated({
			.LevelHandler = _levelHandler,
			.Pos = Vector3i(200, 100, ILevelHandler::MainPlaneZ),
			.Params = params
		});
		_levelHandler->AddActor(std::shared_ptr<Actors::ActorBase>(testActor));

		auto testActor2 = _levelHandler->EventSpawner()->SpawnEvent((EventType)6666, params, Actors::ActorState::None, Vector3i(400, 100, ILevelHandler::MainPlaneZ));
		_levelHandler->AddActor(testActor2);
	}

	bool LevelScripts::AddScriptFromFile(const StringView& path)
	{
		auto s = fs::Open(path, FileAccessMode::Read);
		if (s->GetSize() <= 0) {
			return false;
		}

		String scriptContent(NoInit, s->GetSize());
		s->Read(scriptContent.data(), s->GetSize());

		SmallVector<String> includes;
		int scriptSize = (int)scriptContent.size();

		// First perform the checks for #if directives to exclude code that shouldn't be compiled
		int pos = 0;
		int nested = 0;
		while (pos < scriptSize) {
			asUINT len = 0;
			asETokenClass t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			if (t == asTC_UNKNOWN && scriptContent[pos] == '#' && (pos + 1 < scriptSize)) {
				int start = pos++;

				// Is this an #if directive?
				t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);

				StringView token = scriptContent.slice(pos, pos + len);
				pos += len;

				if (token == "if"_s) {
					t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
					if (t == asTC_WHITESPACE) {
						pos += len;
						t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
					}

					if (t == asTC_IDENTIFIER) {
						StringView word = scriptContent.slice(pos, pos + len);

						// Overwrite the #if directive with space characters to avoid compiler error
						pos += len;

						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}

						// Has this identifier been defined by the application or not?
						if (_definedWords.find(String::nullTerminatedView(word)) == _definedWords.end()) {
							// Exclude all the code until and including the #endif
							pos = ExcludeCode(scriptContent, pos);
						} else {
							nested++;
						}
					}
				} else if (token == "endif"_s) {
					// Only remove the #endif if there was a matching #if
					if (nested > 0) {
						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}
						nested--;
					}
				}
			} else
				pos += len;
		}

		pos = 0;
		while (pos < scriptSize) {
			asUINT len = 0;
			asETokenClass t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			if (t == asTC_COMMENT || t == asTC_WHITESPACE) {
				pos += len;
				continue;
			}

			StringView token = scriptContent.slice(pos, pos + len);

			// Is this a preprocessor directive?
			if (token == "#"_s && (pos + 1 < scriptSize)) {
				int start = pos++;

				t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
				if (t == asTC_IDENTIFIER) {
					token = scriptContent.slice(pos, pos + len);
					if (token == "include"_s) {
						pos += len;
						t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
						if (t == asTC_WHITESPACE) {
							pos += len;
							t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
						}

						if (t == asTC_VALUE && len > 2 && (scriptContent[pos] == '"' || scriptContent[pos] == '\'')) {
							// Get the include file
							includes.push_back(String(&scriptContent[pos + 1], len - 2));
							pos += len;

							// Overwrite the include directive with space characters to avoid compiler error
							for (int i = start; i < pos; i++) {
								if (scriptContent[i] != '\n') {
									scriptContent[i] = ' ';
								}
							}
						}
					} else if (token == "pragma"_s) {
						// Read until the end of the line
						pos += len;
						for (; pos < scriptSize && scriptContent[pos] != '\n'; pos++);

						// TODO: Call the pragma callback
						/*string pragmaText(&scriptContent[start + 7], pos - start - 7);
						int r = pragmaCallback ? pragmaCallback(pragmaText, *this, pragmaParam) : -1;
						if (r < 0) {
							// TODO: Report the correct line number
							_engine->WriteMessage(sectionname, 0, 0, asMSGTYPE_ERROR, "Invalid #pragma directive");
							return r;
						}*/

						// Overwrite the pragma directive with space characters to avoid compiler error
						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}
					}
				} else {
					// Check for lines starting with #!, e.g. shebang interpreter directive. These will be treated as comments and removed by the preprocessor
					if (scriptContent[pos] == '!') {
						// Read until the end of the line
						pos += len;
						for (; pos < scriptSize && scriptContent[pos] != '\n'; pos++);

						// Overwrite the directive with space characters to avoid compiler error
						for (int i = start; i < pos; i++) {
							if (scriptContent[i] != '\n') {
								scriptContent[i] = ' ';
							}
						}
					}
				}
			} else {
				// Don't search for metadata/includes within statement blocks or between tokens in statements
				pos = SkipStatement(scriptContent, pos);
			}
		}

		// Build the actual script
		_engine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, true);
		_module->AddScriptSection(path.data(), scriptContent.data(), scriptSize, 0);

		if (includes.size() > 0) {
			// Try to load the included file from the relative directory of the current file
			String currentDir = fs::GetDirectoryName(path);

			// Load the included scripts
			for (auto& include : includes) {
				if (!AddScriptFromFile(fs::JoinPath(currentDir, include[0] == '/' || include[0] == '\\' ? include.exceptPrefix(1) : StringView(include)))) {
					return false;
				}
			}
		}

		return true;
	}

	int LevelScripts::ExcludeCode(String& scriptContent, int pos)
	{
		int scriptSize = (int)scriptContent.size();
		asUINT len = 0;
		int nested = 0;

		while (pos < scriptSize) {
			_engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			if (scriptContent[pos] == '#') {
				scriptContent[pos] = ' ';
				pos++;

				// Is it an #if or #endif directive?
				_engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);

				StringView token = scriptContent.slice(pos, pos + len);

				if (token == "if"_s) {
					nested++;
				} else if (token == "endif"_s) {
					if (nested-- == 0) {
						pos += len;
						break;
					}
				}

				for (uint32_t i = pos; i < pos + len; i++) {
					if (scriptContent[i] != '\n') {
						scriptContent[i] = ' ';
					}
				}
			} else if (scriptContent[pos] != '\n') {
				for (uint32_t i = pos; i < pos + len; i++) {
					if (scriptContent[i] != '\n') {
						scriptContent[i] = ' ';
					}
				}
			}
			pos += len;
		}

		return pos;
	}

	int LevelScripts::SkipStatement(String& scriptContent, int pos)
	{
		int scriptSize = (int)scriptContent.size();
		asUINT len = 0;

		// Skip until ; or { whichever comes first
		while (pos < scriptSize && scriptContent[pos] != ';' && scriptContent[pos] != '{') {
			_engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
			pos += len;
		}

		// Skip entire statement block
		if (pos < scriptSize && scriptContent[pos] == '{') {
			pos += 1;

			// Find the end of the statement block
			int level = 1;
			while (level > 0 && pos < scriptSize) {
				asETokenClass t = _engine->ParseToken(&scriptContent[pos], scriptSize - pos, &len);
				if (t == asTC_KEYWORD) {
					if (scriptContent[pos] == '{') {
						level++;
					} else if (scriptContent[pos] == '}') {
						level--;
					}
				}

				pos += len;
			}
		} else {
			pos += 1;
		}
		return pos;
	}

	void LevelScripts::Status(const asSMessageInfo& msg)
	{
		switch (msg.type) {
			case asMSGTYPE_ERROR: LOGE_X("%s (%i, %i): %s", msg.section, msg.row, msg.col, msg.message); break;
			case asMSGTYPE_WARNING: LOGW_X("%s (%i, %i): %s", msg.section, msg.row, msg.col, msg.message); break;
			default: LOGI_X("%s (%i, %i): %s", msg.section, msg.row, msg.col, msg.message); break;
		}
	}

	void LevelScripts::asRegisterSpawnable(int eventType, String& typeName)
	{
		// TODO
		auto ctx = asGetActiveContext();
		auto controller = reinterpret_cast<LevelScripts*>(ctx->GetUserData(LevelScripts::ContextToController));

		controller->_eventTypeToTypeName.emplace(eventType, typeName);

		controller->_levelHandler->EventSpawner()->RegisterSpawnable((EventType)eventType, [](const Actors::ActorActivationDetails& details) -> std::shared_ptr<Actors::ActorBase> {
			if (auto levelHandler = dynamic_cast<LevelHandler*>(details.LevelHandler)) {
				auto _this = levelHandler->_scripts.get();
				auto it = _this->_eventTypeToTypeName.find((int)details.Type);
				if (it != _this->_eventTypeToTypeName.end()) {
					auto actor = _this->CreateActorInstance(it->second);
					actor->OnActivated(details);
					return std::shared_ptr<Actors::ActorBase>(actor);
				}
			}
			return nullptr;
		});
	}

	Actors::ActorBase* LevelScripts::CreateActorInstance(const StringView& typeName)
	{
		auto nullTerminatedTypeName = String::nullTerminatedView(typeName);

		// Create an instance of the ActorBase script class that inherits from the ScriptActorWrapper C++ class
		asITypeInfo* typeInfo = _module->GetTypeInfoByName(nullTerminatedTypeName.data());
		asIScriptObject* obj = reinterpret_cast<asIScriptObject*>(_engine->CreateScriptObject(typeInfo));

		// Get the pointer to the C++ side of the ActorBase class
		ScriptActorWrapper* obj2 = *reinterpret_cast<ScriptActorWrapper**>(obj->GetAddressOfProperty(0));

		// Increase the reference count to the C++ object, as this is what will be used to control the life time of the object from the application side 
		obj2->AddRef();

		// Release the reference to the script side
		obj->Release();

		return obj2;
	}
}

#endif