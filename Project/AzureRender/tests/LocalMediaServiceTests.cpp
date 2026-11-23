#include "app/LocalMediaService.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/AudioRuntime.hpp"
#include "assets/image/ImageCodec.hpp"
#include "assets/image/ImageEncoder.hpp"
#include "platform/NativeArguments.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace azurerender;
namespace fs = std::filesystem;
using Json = nlohmann::json;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void wave(const fs::path& path) {
    std::ofstream output(path,std::ios::binary);
    auto le=[&](unsigned value,unsigned bytes){for(unsigned i=0;i<bytes;++i)output.put(static_cast<char>(value>>(8*i)));};
    output.write("RIFF",4);le(36+16000,4);output.write("WAVEfmt ",8);le(16,4);le(1,2);le(1,2);
    le(8000,4);le(16000,4);le(2,2);le(16,2);output.write("data",4);le(16000,4);
    for(unsigned i=0;i<8000;++i)le(i%40<20?10000:static_cast<unsigned>(-10000),2);
}
Json wait(LocalMediaService& service) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = service.status();
        if (status[0] != "running") return status;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Media task exceeded its deadline");
}
int main(int argc, char** argv) try {
    const auto args = nativeArguments(argc, argv);
    require(args.size() == 2 || args.size() == 4, "Expected managed output and optional original WAV/PNG fixtures");
    const auto root = fs::absolute(fs::u8path(args[1])) /
        ("case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "sources/nested");
    const auto audioInput=args.size()==4?fs::u8path(args[2]):root/"original.wav";
    const auto imageInput=args.size()==4?fs::u8path(args[3]):root/"original.png";
    if(args.size()==2){wave(audioInput);const unsigned char pixels[]{255,0,0,255,0,255,0,255};writeRgba8Png(imageInput,2,1,pixels);}
    fs::copy_file(audioInput, root / "sources/a.wav");
    fs::copy_file(audioInput, root / "sources/nested/copy.wav");
    fs::copy_file(imageInput, root / "sources/image.png");
    std::ofstream(root / "sources/broken.wav") << "RIFF truncated";
    auto store = std::make_shared<ProjectSaveStore>(root / "saves", "media-service");
    bool directory = true;
    LocalMediaService service(store, [&](const PathSelectionRequest& request) {
        require(request.directory == directory && request.multiple, "Selection must preserve native multi-select and directory semantics");
        require(!request.extensions.empty(), "Media selection must declare its supported extensions");
        PathSelectionResult selected;
        selected.paths = {directory ? root / "sources" : root / "sources/image.png"};
        selected.path = selected.paths.front();
        return selected;
    });
    require(service.entries("music").empty() && !fs::exists(root / "saves"), "Service queries must be read-only");
    require(service.select("music", true), "Directory selection must start an import task");
    require(!service.startPaths("music", {root / "sources/a.wav"}, false), "An owned task must reject overlapping starts");
    const auto audioStatus = wait(service);
    require(audioStatus[0] == "completed" && audioStatus[1] == 3 && audioStatus[2] == 3 && audioStatus[3] == 1,
        "Directory tasks must retain valid files and expose per-file failures");
    const auto audio = service.entries("music");
    require(audio.size() == 1 && audio[0][3] == true, "Duplicate bytes must yield one available media entry");
    Project project; project.file = root / "project.azureproject";
    AssetDatabase assets(project);
    assets.setReferenceResolver([&](const std::string& reference) { return service.resolve(reference); });
    const auto reference = audio[0][2].get<std::string>();
    const auto path = assets.resolveReference(reference);
    require(fs::is_regular_file(path) && AssetDatabase(assets).resolveReference(reference) == path,
        "Asset database copies must preserve controlled runtime resolution");
    AudioRuntime mixer(false); const auto sound = mixer.load(path, false, 1); mixer.play(sound);
    double energy = 0; for (const auto sample : mixer.mix(4096)) energy += sample * sample;
    require(energy > 1, "Imported references must reach the production audio mixer"); mixer.release(sound);
    directory = false;
    require(service.select("images", false) && wait(service)[3] == 0, "Image selection must use the shared task contract");
    const auto image = service.entries("images");
    require(image.size() == 1 && decodeImageFile(assets.resolveReference(image[0][2])).width > 0,
        "Imported image references must reach the production decoder");
    fs::remove(root / "sources/a.wav"); fs::remove(root / "sources/nested/copy.wav"); fs::remove(root / "sources/image.png");
    LocalMediaService reopened(store);
    require(reopened.entries("music").size() == 1 && reopened.resolve(reference) == path,
        "Reopened service must retain imported media without its source directory");
    bool invalid = false;
    try { static_cast<void>(reopened.resolve("media:/music/../../outside")); } catch (const std::exception&) { invalid = true; }
    require(invalid && !reopened.resolve("assets:/ordinary.png"), "Runtime resolution must accept only committed media identities");
    LocalMediaService cancelled(store, [](const auto&) { PathSelectionResult result; result.cancelled = true; return result; });
    require(!cancelled.select("music", false) && cancelled.status()[0] == "cancelled", "Native selection cancellation must remain observable");
    require(service.startPaths("music", {audioInput}, false), "A completed task must permit a new import");
    service.cancel(); const auto stopped = wait(service);
    require(stopped[0] == "cancelled" || stopped[0] == "completed", "Cancellation must settle its owned task");
    std::weak_ptr<ProjectSaveStore> retired;
    {
        auto owned=std::make_shared<ProjectSaveStore>(root/"closing-saves","closing");retired=owned;
        LocalMediaService closing(owned);owned.reset();
        require(!retired.expired()&&closing.startPaths("music",{audioInput},false), "An active service must retain its storage owner");
    }
    require(retired.expired(), "Closing must cancel and join its worker before releasing storage ownership");
    store->write("local-media-music", Json::array({99, Json::array()}));
    require(service.startPaths("music", {audioInput}, false), "The task must expose version failure asynchronously");
    require(wait(service)[0] == "failed" && store->read("local-media-music")[0] == 99, "Future versions must preserve metadata and report task failure");
    std::cout << "Runtime media selection, worker import, partial failures, cancellation, reopening and audio/image references passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
