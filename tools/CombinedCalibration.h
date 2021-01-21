#include "../tools/Tool.h"

#if __cplusplus < 201402
namespace std
{
template <size_t... Is>
struct index_sequence
{
};

template <size_t N, size_t... Is>
struct make_index_sequence_impl
{
    using type = typename make_index_sequence_impl<N - 1, N - 1, Is...>::type;
};

template <size_t N, size_t... Is>
struct make_index_sequence_impl<N, 0, Is...>
{
    using type = index_sequence<0, Is...>;
};

template <size_t N>
using make_index_sequence = typename make_index_sequence_impl<N>::type;
} // namespace std
#endif

template <class... Tools>
struct CombinedCalibration : public Tool
{
  public:
    static constexpr int size = sizeof...(Tools);

    CombinedCalibration() : current_tool(this) {}

    void Start(int run) { start_impl(run, std::make_index_sequence<size>()); }

    void Configure(std::string cHWFile, bool enableStream = false) override
    {
        Tool::Configure(cHWFile, enableStream);
        Tool::CreateResultDirectory("Results", false, false);
    }

    void Stop() override
    {
        Tool::dumpConfigFiles();
        Tool::SaveResults();
        Tool::Destroy();
    }

  private:
    template <size_t... Is>
    void start_impl(int run, std::index_sequence<Is...>)
    {
        __attribute__((unused)) auto _ = {(start_single(run, std::get<Is>(tools)), 0)...};
    }

    template <class T>
    void start_single(int run, T& tool)
    {
        tool.Inherit(current_tool);
        tool.ConfigureCalibration();
        tool.Start(run);
        tool.resetPointers();
        current_tool = &tool;
    }

    Tool*                current_tool;
    std::tuple<Tools...> tools;
};
