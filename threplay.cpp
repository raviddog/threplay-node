#include <napi.h>
#include <stdlib.h>
#include "zuntypes.h"

unsigned int th06_decrypt(unsigned char* buf, char key, unsigned int length);
void th_decrypt(unsigned char * buffer, int length, int block_size, unsigned char base, unsigned char add);
unsigned int th_unlzss(unsigned char * buffer, unsigned char * decode, unsigned int length);

void get_th06(Napi::Object& out, uint8_t* buf, size_t len, Napi::Env& env) {
	out.Set("game", "th06");
	
	th06_replay_header_t* rep_raw = (th06_replay_header_t*)buf;
	size_t size = len - offsetof(th06_replay_header_t, crypted_data);
	uint8_t* rep_dec = (uint8_t*)malloc(size);
	memcpy(rep_dec, rep_raw->crypted_data, size);
	th06_decrypt(rep_dec, rep_raw->key, size);
	
	th06_replay_t* rep = (th06_replay_t*)rep_dec;
	
	// Replay name
	out.Set("name", rep->name);
	// Date
	out.Set("date", rep->date);
	// Score
	out.Set("score", rep->score);
	out.Set("slowdown", rep->slowdown);
	
	Napi::Array stages = Napi::Array::New(env);
	
	for(int i = 0; i < 6; i++) {
		if(rep->stage_offsets[i]) {
			Napi::Object stage_ = Napi::Object::New(env);
			th06_replay_stage_t* stage = (th06_replay_stage_t*)(rep_dec + rep->stage_offsets[i] - offsetof(th06_replay_header_t, crypted_data));
				
			stage_.Set("stage", i + 1);
			stage_.Set("score", stage->score);
			stage_.Set("power", stage->power);
			stage_.Set("lives", stage->lives);
			stage_.Set("bombs", stage->bombs);

			stages.Set(i, stage_);
		}
	}
	out.Set("stages", stages);	
}

void get_th17(Napi::Object& out, uint8_t* buf, size_t len, Napi::Env& env) {
	out.Set("game", "th17");
	
	th17_replay_header_t* rep_raw = (th17_replay_header_t*)buf;
	uint8_t* comp_buf = (uint8_t*)malloc(rep_raw->comp_size);
	uint8_t* rep_dec = (uint8_t*)malloc(rep_raw->size);
	memcpy((void*)comp_buf, (void*)(buf + sizeof(th17_replay_header_t)), rep_raw->comp_size);
	th_decrypt(comp_buf, rep_raw->comp_size, 0x400, 0x5c, 0xe1);
	th_decrypt(comp_buf, rep_raw->comp_size, 0x100, 0x7d, 0x3a);
	th_unlzss(comp_buf, rep_dec, rep_raw->comp_size);
	free(comp_buf);
	
	th17_replay_t* rep = (th17_replay_t*)rep_dec;
	
	if(rep->spell_practice_id != -1) {
		out.Set("spell_practice_id", rep->spell_practice_id);
		return;
	}
	
	out.Set("name", rep->name);
	out.Set("date", rep->timestamp);
	out.Set("difficulty", rep->difficulty);
	out.Set("score", rep->score * 10);

	Napi::Array stages = Napi::Array::New(env);
	size_t stage_off = sizeof(th17_replay_t);
	for(int i = 0; i < rep->stage_count; i++) {
		Napi::Object stage_ = Napi::Object::New(env);
		th17_replay_stage_t* stage = (th17_replay_stage_t*)((size_t)rep + stage_off);
				
		stage_.Set("stage", stage->stagedata.stage_num);
		stage_.Set("score", stage->stagedata.score);
		stage_.Set("graze", stage->stagedata.graze);
		stage_.Set("misscount", stage->stagedata.miss_count);
		stage_.Set("piv", stage->stagedata.piv);
		stage_.Set("power", stage->stagedata.power);
		stage_.Set("lifes", stage->stagedata.lifes);
		stage_.Set("life_pieces", stage->stagedata.life_pieces);
		stage_.Set("bombs", stage->stagedata.bombs);
		stage_.Set("bomb_pieces", stage->stagedata.bomb_pieces);
		
		Napi::Array tokens = Napi::Array::New(env);
		for(int j = 0; j < 5; j++) {
			tokens.Set(j, stage->stagedata.tokens[j]);
		}
		stage_.Set("tokens", tokens);
		
		stages.Set(i, stage_);
		stage_off += stage->end_off + sizeof(th17_replay_stage_t);
	}
	out.Set("stages", stages);
}

Napi::Value get_replay_data(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	Napi::Object ret = Napi::Object::New(env);
	
	if(!info[0].IsBuffer()) {
		Napi::TypeError::New(env, "Wrong argument type: argument 0 must be a Buffer").ThrowAsJavaScriptException();
    	return env.Null();	
	}
	
	uint8_t* buf = info[0].As<Napi::Buffer<uint8_t>>().Data();
	size_t len = info[0].As<Napi::Buffer<uint8_t>>().Length();
	
	uint32_t magic = *(uint32_t*)buf;
	
	switch(magic) {	
	case 0x50523654: // "T6RP"
		get_th06(ret, buf, len, env);
		break;
		
	case 0x72373174: // "t17r"
		get_th17(ret, buf, len, env);
		break;
	
	default:
		break;
	}
	return ret;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set(Napi::String::New(env, "get_replay_data"),
				Napi::Function::New(env, get_replay_data));
	return exports;
}

NODE_API_MODULE(threplay, Init)