#include "nan.h"
#include "../../include/models/Candlestick.h"

void Candlestick::CreateFromArray(
	std::vector<Candlestick>& candlesticks,
	const v8::Local<v8::Array>& input) {

	unsigned candlestickCount = input->Length();

	candlesticks.clear();

	for (unsigned i = 0; i < candlestickCount; i++) {

		//All members of an array have to be integers
		/*if (!input->Get(i)->IsObject()) {
			Nan::ThrowTypeError(v8::String::Concat(Nan::New<v8::String>("Wrong type of member in array on index: ").ToLocalChecked(),
				Nan::New<v8::Int32>(i)->ToString()));
			return;
		}*/

		v8::Local<v8::Object> candlestick = v8::Local<v8::Object>::Cast(input->Get(i));

		Candlestick c = CreateFromObject(candlestick);

		candlesticks.push_back(c);
	}
}

Candlestick Candlestick::CreateFromObject(
	const v8::Local<v8::Object>& input) {

	Candlestick c = Candlestick();

	c.Open = input->Get(Nan::New<v8::String>("open").ToLocalChecked())->NumberValue();
	c.Low = input->Get(Nan::New<v8::String>("low").ToLocalChecked())->NumberValue();
	c.High = input->Get(Nan::New<v8::String>("high").ToLocalChecked())->NumberValue();
	c.Close = input->Get(Nan::New<v8::String>("close").ToLocalChecked())->NumberValue();
	c.Time = input->Get(Nan::New<v8::String>("time").ToLocalChecked())->Int32Value();

	return c;
}

void Candlestick::ToArray(
	const std::vector<Candlestick>& candlesticks,
	v8::Local<v8::Array>& output) {

	size_t candlestickCount = candlesticks.size();

	for (size_t i = 0; i < candlestickCount; i++) {

		auto object = Nan::New<v8::Object>();

		const Candlestick& candlestick = candlesticks[i];

		ToObject(candlestick, object);

		output->Set(static_cast<uint32_t>(i), v8::Handle<v8::Value>::Cast(object));
	}
}

void Candlestick::ToObject(
	const Candlestick& candlestick,
	v8::Local<v8::Object>& output) {

	output->Set(Nan::New<v8::String>("open").ToLocalChecked(),
		Nan::New<v8::Number>(candlestick.Open));
	output->Set(Nan::New<v8::String>("low").ToLocalChecked(),
		Nan::New<v8::Number>(candlestick.Low));
	output->Set(Nan::New<v8::String>("high").ToLocalChecked(),
		Nan::New<v8::Number>(candlestick.High));
	output->Set(Nan::New<v8::String>("close").ToLocalChecked(),
		Nan::New<v8::Number>(candlestick.Close));
	output->Set(Nan::New<v8::String>("time").ToLocalChecked(),
		Nan::New<v8::Int32>(candlestick.Time));
}
