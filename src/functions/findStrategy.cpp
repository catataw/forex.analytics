#include <nan.h>
#include <node.h>
#include "../../include/TradingSystem.h"
#include "../../include/indicators/Indicator.h"

const int DEFAULT_POPULATION_COUNT = 100;
const int DEFAULT_GENERATION_COUNT = 100;
const int DEFAULT_SELECTION_AMOUNT = 10;

const double DEFAULT_LEAF_VALUE_MUTATION_PROBABILITY = 0.5;
const double DEFAULT_LEAF_SIGN_MUTATION_PROBABILITY = 0.3;
const double DEFAULT_LOGICAL_NODE_MUTATION_PROBABILITY = 0.3;
const double DEFAULT_LEAF_INDICATOR_MUTATION_PROBABILITY = 0.2;
const double DEFAULT_CROSSOVER_PROBABILITY = 0.03;

// the 'baton' is the carrier for data between functions
struct FindStrategyBaton
{
	Nan::Callback* callback;

	BinaryTreeChromosome* chromosome;
	std::vector<Candlestick> candlesticks;
	std::vector<BaseIndicator*> indicators;
	const char* errorMessage;

	unsigned populationCount;
	unsigned generationCount;
	unsigned selectionAmount;
	double leafValueMutationProbability;
	double leafSignMutationProbability;
	double logicalNodeMutationProbability;
	double leafIndicatorMutationProbability;
	double crossoverProbability;
};

struct StrategyUpdateBaton
{
	BinaryTreeChromosome* chromosome;
	double fitness;
	int generation;
};

template <typename T>
static void chromosomeToObject(T* baton, v8::Local<v8::Object>& strategy)
{
	v8::Local<v8::Object> buy = Nan::New<v8::Object>();
	v8::Local<v8::Object> sell = Nan::New<v8::Object>();

	baton->chromosome->buy->ToJs(buy);
	baton->chromosome->sell->ToJs(sell);

	strategy = Nan::New<v8::Object>();
	strategy->Set(Nan::New<v8::String>("buy").ToLocalChecked(), buy);
	strategy->Set(Nan::New<v8::String>("sell").ToLocalChecked(), sell);
}

class FindStrategyAsyncWorker : public Nan::AsyncProgressWorker
{
public:
	explicit FindStrategyAsyncWorker(
		Nan::Callback* callback_,
		FindStrategyBaton* baton_)
		: AsyncProgressWorker(callback_)
	{
		this->baton = baton_;
	}

	void Execute(const ExecutionProgress& progress) override
	{
		TradingSystem system;
		Nan::Callback* callback = this->baton->callback;
		ExecutionProgress* pp = &const_cast<ExecutionProgress&>(progress);

		auto update = [callback, pp](double fitness, BinaryTreeChromosome* chromosome, int generation)
		{
			if (callback != nullptr)
			{
				StrategyUpdateBaton* updateBaton = new StrategyUpdateBaton;
				updateBaton->fitness = fitness;
				updateBaton->chromosome = chromosome;
				updateBaton->generation = generation;

				pp->Send(reinterpret_cast<const char*>(updateBaton), sizeof(StrategyUpdateBaton));
			}
		};

		try
		{
			this->baton->chromosome = system.PerformAnalysis(
				baton->candlesticks,
				baton->indicators,
				baton->populationCount,
				baton->generationCount,
				baton->selectionAmount,
				baton->leafValueMutationProbability,
				baton->leafSignMutationProbability,
				baton->logicalNodeMutationProbability,
				baton->leafIndicatorMutationProbability,
				baton->crossoverProbability,
				update);
		}
		catch (const char* error)
		{
			baton->errorMessage = error;
			baton->chromosome = nullptr;
		}
	}

	virtual void HandleOKCallback() override {
		Nan::HandleScope scope;

		if (baton->chromosome == nullptr) {

			this->GetFromPersistent("resolver").As<v8::Promise::Resolver>()
				->Reject(Nan::Error(baton->errorMessage));
		}
		else {
			v8::Local<v8::Object> strategy;

			chromosomeToObject(baton, strategy);

			this->GetFromPersistent("resolver").As<v8::Promise::Resolver>()
				->Resolve(strategy);
		}
	}

	virtual void HandleErrorCallback() override {
		Nan::HandleScope scope;

		this->GetFromPersistent("resolver").As<v8::Promise::Resolver>()
			->Reject(v8::Exception::Error(
				Nan::New<v8::String>(ErrorMessage()).ToLocalChecked()));
	}

	void HandleProgressCallback(const char* data, size_t size) override
	{
		Nan::HandleScope scope;

		StrategyUpdateBaton* updateBaton = reinterpret_cast<StrategyUpdateBaton*>(const_cast<char*>(data));

		v8::Local<v8::Object> strategy;

		chromosomeToObject(updateBaton, strategy);

		v8::Handle<v8::Value> argv[] =
		{
			strategy,
			v8::Handle<v8::Value>(Nan::New<v8::Number>(updateBaton->fitness)),
			v8::Handle<v8::Value>(Nan::New<v8::Int32>(updateBaton->generation)),
		};

		this->baton->callback->Call(3, argv);
	}

private:
	FindStrategyBaton* baton;
};

bool findStrategyValidateInput(const Nan::FunctionCallbackInfo<v8::Value>& args, v8::Local<v8::Promise::Resolver> resolver)
{
	if (args.Length() < 1)
	{
		Nan::ThrowTypeError("Wrong number of arguments");
		return false;
	}

	if (!args[0]->IsArray())
	{
		Nan::ThrowTypeError("Wrong first argument. Expecting array of candlesticks");
		return false;
	}

	if (args.Length() > 1 && (!args[1]->IsObject() || args[1]->IsArray()))
	{
		Nan::ThrowTypeError("Wrong second argument. Expecting object with genetic algorithm configuration");
		return false;
	}

	if (args.Length() > 2 && (!args[2]->IsFunction()))
	{
		Nan::ThrowTypeError("Wrong third argument. Expecting a function");
		return false;
	}

	return true;
}

int getIntOrDefault(v8::Handle<v8::Object> object, const char* name, int def)
{
	if (!object->Has(Nan::New<v8::String>(name).ToLocalChecked()))
		return def;

	return object->Get(Nan::New<v8::String>(name).ToLocalChecked())->Int32Value();
}

double getNumberOrDefault(v8::Handle<v8::Object> object, const char* name, double def)
{
	if (!object->Has(Nan::New<v8::String>(name).ToLocalChecked()))
		return def;

	return object->Get(Nan::New<v8::String>(name).ToLocalChecked())->NumberValue();
}

v8::Handle<v8::Object> getObjectFromArguments(const Nan::FunctionCallbackInfo<v8::Value>& args, int index)
{
	if (args.Length() - 1 >= index)
		return v8::Handle<v8::Object>::Cast(args[index]);

	return Nan::New<v8::Object>();
}

v8::Handle<v8::Array> getArrayFromArguments(const Nan::FunctionCallbackInfo<v8::Value>& args, int index)
{
	if (args.Length() - 1 >= index)
		return v8::Handle<v8::Array>::Cast(args[index]);

	return Nan::New<v8::Array>();
}


NAN_METHOD(findStrategy)
{
	v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(info.GetIsolate());

	if (findStrategyValidateInput(info, resolver))
	{
		std::vector<Candlestick> candlesticks;
		std::vector<std::string> indicatorNames;
		std::vector<BaseIndicator *> indicators;

		v8::Handle<v8::Array> candlestickArray = getArrayFromArguments(info, 0);
		v8::Handle<v8::Object> configuration = getObjectFromArguments(info, 1);

		int populationCount = getIntOrDefault(
			configuration, "populationCount", DEFAULT_POPULATION_COUNT);
		int generationCount = getIntOrDefault(
			configuration, "generationCount", DEFAULT_GENERATION_COUNT);
		int selectionAmount = getIntOrDefault(
			configuration, "selectionAmount", DEFAULT_SELECTION_AMOUNT);

		double leafValueMutationProbability = getNumberOrDefault(
			configuration, "leafValueMutationProbability",
			DEFAULT_LEAF_VALUE_MUTATION_PROBABILITY);

		double leafSignMutationProbability = getNumberOrDefault(
			configuration, "leafSignMutationProbability",
			DEFAULT_LEAF_SIGN_MUTATION_PROBABILITY);

		double logicalNodeMutationProbability = getNumberOrDefault(
			configuration, "logicalNodeMutationProbability",
			DEFAULT_LOGICAL_NODE_MUTATION_PROBABILITY);

		double leafIndicatorMutationProbability = getNumberOrDefault(
			configuration, "leafIndicatorMutationProbability",
			DEFAULT_LEAF_INDICATOR_MUTATION_PROBABILITY);

		double crossoverProbability = getNumberOrDefault(
			configuration, "crossoverProbability",
			DEFAULT_CROSSOVER_PROBABILITY);

		//If the indicator array is present use it to create indicators
		if (configuration->Has(Nan::New<v8::String>("indicators").ToLocalChecked()))
		{
			v8::Handle<v8::Array> indicatorArray = v8::Handle<v8::Array>::Cast(configuration->Get(Nan::New<v8::String>("indicators").ToLocalChecked()));

			for (unsigned i = 0; i < indicatorArray->Length(); i++)
			{
				BaseIndicator* indicator = IndicatorFactory::Create(std::string(*v8::String::Utf8Value(indicatorArray->Get(i)->ToString())));
				indicators.push_back(indicator);
			}
		}
		else
		{
			//By default use all indicators
			indicators = IndicatorFactory::CreateAll();
		}

		//Fill candlesticks from the candlestick array
		Candlestick::CreateFromArray(candlesticks, candlestickArray);

		FindStrategyBaton* baton = new FindStrategyBaton;

		//Fill the baton with all data required for the calculation
		baton->candlesticks = candlesticks;
		baton->indicators = indicators;
		baton->populationCount = populationCount;
		baton->generationCount = generationCount;
		baton->selectionAmount = selectionAmount;
		baton->leafValueMutationProbability = leafValueMutationProbability;
		baton->leafSignMutationProbability = leafSignMutationProbability;
		baton->logicalNodeMutationProbability = logicalNodeMutationProbability;
		baton->leafIndicatorMutationProbability = leafIndicatorMutationProbability;
		baton->crossoverProbability = crossoverProbability;

		if (info.Length() > 2)
		{
			baton->callback = new Nan::Callback(info[2].As<v8::Function>());
		}
		else
		{
			baton->callback = nullptr;
		}

		Nan::Callback* callback = new Nan::Callback();

		auto worker = new FindStrategyAsyncWorker(callback, baton);
		worker->SaveToPersistent("resolver", resolver);

		Nan::AsyncQueueWorker(worker);
	}

	//Return a promise
	info.GetReturnValue().Set(resolver->GetPromise());
} // findStrategy
