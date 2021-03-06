#include "listener.hpp"
#include <nan.h>
#include <uv.h>
#include <queue>
#include <mutex>

uv_async_t gotPacket;
Nan::Persistent<v8::Function> callback;
Listener listener;

std::queue<Packet> packets;
std::mutex packetsMutex;
long packetCounter = 0;

void AddPacket(const Packet packet)
{
	packets.push(packet);
}

bool GetPacket(Packet& packet)
{
	if(packets.empty())
		return false;

	packet = packets.front();
	packets.pop();
	return true;
}

void GotPacket(uv_async_t *handle)
{
	packetsMutex.lock();
	v8::Isolate *isolate = v8::Isolate::GetCurrent();
	v8::Local<v8::Array> result = Nan::New<v8::Array>();
	int resultSize = 0;

	Packet packet;
	while(GetPacket(packet))
	{
		packetCounter++;
		v8::Local<v8::Object> packetObject = Nan::New<v8::Object>();
		packetObject->Set(isolate->GetCurrentContext(), Nan::New("protocol").ToLocalChecked(), Nan::New(packet.protocol));
		packetObject->Set(isolate->GetCurrentContext(), Nan::New("id").ToLocalChecked(), Nan::New(packetCounter));
		packetObject->Set(isolate->GetCurrentContext(), Nan::New("size").ToLocalChecked(), Nan::New(packet.data_size));
		packetObject->Set(isolate->GetCurrentContext(), Nan::New("from").ToLocalChecked(), Nan::New(packet.from).ToLocalChecked());
		packetObject->Set(isolate->GetCurrentContext(), Nan::New("to").ToLocalChecked(), Nan::New(packet.to).ToLocalChecked());
		packetObject->Set(isolate->GetCurrentContext(), Nan::New("data").ToLocalChecked(), Nan::New(packet.readable_data).ToLocalChecked());
		packetObject->Set(isolate->GetCurrentContext(), Nan::New("hex").ToLocalChecked(), Nan::New(packet.hex_data).ToLocalChecked());

		result->Set(isolate->GetCurrentContext(), resultSize, packetObject);
		resultSize++;
	}

	const int argc = 1;
	v8::Local<v8::Value> argv[argc] = {result};
	v8::Local<v8::Function>::New(isolate, callback)->Call(isolate->GetCurrentContext(), v8::Null(isolate), argc, argv);
	packetsMutex.unlock();
}

void StartListen(const Nan::FunctionCallbackInfo<v8::Value> &info)
{
	v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
	if (info.Length() < 2 || !info[0]->IsNumber() || !info[1]->IsFunction())
	{
		return;
	}

	listener.stop();
	unsigned int ip = info[0]->NumberValue(context).FromJust();
	v8::Local<v8::Function> local = v8::Local<v8::Function>::Cast(info[1]);
	callback.Reset(local);

	listener.listen(ip, [](Packet packet) {
		packetsMutex.lock();
		AddPacket(packet);
		uv_async_send(&gotPacket);
		packetsMutex.unlock();
	});

	uv_async_init(uv_default_loop(), &gotPacket, GotPacket);
	info.GetReturnValue().Set(Nan::New(ip));
}

void StopListen(const Nan::FunctionCallbackInfo<v8::Value> &info)
{
	listener.stop();
}

void GetHostname(const Nan::FunctionCallbackInfo<v8::Value> &info)
{
	std::string hostname = Listener::get_hostname();
	info.GetReturnValue().Set(Nan::New(hostname).ToLocalChecked());
}

void GetIPs(const Nan::FunctionCallbackInfo<v8::Value> &info)
{
	v8::Isolate *isolate = v8::Isolate::GetCurrent();
	std::vector<unsigned int> ips = Listener::get_ips();
	v8::Local<v8::Array> result = Nan::New<v8::Array>(ips.size());
	for (int i = 0; i < ips.size(); i++)
	{
		v8::Local<v8::Object> ip = Nan::New<v8::Object>();
		ip->Set(isolate->GetCurrentContext(), Nan::New("string").ToLocalChecked(), Nan::New(ip_to_string(ips[i])).ToLocalChecked());
		ip->Set(isolate->GetCurrentContext(), Nan::New("key").ToLocalChecked(), Nan::New(ips[i]));
		result->Set(isolate->GetCurrentContext(), i, ip);
	}

	info.GetReturnValue().Set(result);
}

NAN_MODULE_INIT(Init)
{
	Nan::Set(target, Nan::New("listen").ToLocalChecked(),
			 Nan::GetFunction(Nan::New<v8::FunctionTemplate>(StartListen)).ToLocalChecked());

	Nan::Set(target, Nan::New("stop").ToLocalChecked(),
			 Nan::GetFunction(Nan::New<v8::FunctionTemplate>(StopListen)).ToLocalChecked());

	Nan::Set(target, Nan::New("getHostname").ToLocalChecked(),
			 Nan::GetFunction(Nan::New<v8::FunctionTemplate>(GetHostname)).ToLocalChecked());

	Nan::Set(target, Nan::New("getIPs").ToLocalChecked(),
			 Nan::GetFunction(Nan::New<v8::FunctionTemplate>(GetIPs)).ToLocalChecked());
}

NODE_MODULE(addon, Init)