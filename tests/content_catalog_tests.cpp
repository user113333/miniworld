#include "content/catalog.h"
#include "content_test_support.h"
#include "test_support.h"

int main()
{
    return test::run("Content catalog", [] {
        AssetTestDirectory temp;
        temp.copy_resources(TEST_RESOURCE_ROOT);
        content::Catalog first, second;
        test::expect(first.initialize(temp.path) && second.initialize(temp.path), "Cannot scan test resource root");
        const auto& assets = first.assets();
        test::expect(assets.size() > 30 && assets.size() == second.assets().size(), "Complete resources are not staged");
        bool source = false, document = false, folder = false;
        for (std::size_t i = 0; i < assets.size(); ++i) {
            const auto& asset = assets[i];
            test::expect(asset.key == second.assets()[i].key && content::valid_key(asset.key), "Catalog order/key is unstable");
            if (i) test::expect(assets[i - 1].key < asset.key, "Duplicate key or unsorted catalog");
            test::expect(first.find(asset.key) == &asset && first.path(asset.key) == temp.path / asset.key,
                         "Lookup is not using a resource-relative key");
            source |= asset.type == content::AssetType::source && asset.status == content::LoadStatus::unsupported && !asset.diagnostic.empty();
            document |= asset.type == content::AssetType::document;
            folder |= asset.type == content::AssetType::folder;
        }
        test::expect(source && document && folder, "Source-only files, licenses or folders are missing");
        for (const char* key : {"map/tree1.png", "map/house.png", "map/ship.png"})
            test::expect(first.find(key) && first.find(key)->type == content::AssetType::image &&
                         first.find(key)->status == content::LoadStatus::unloaded, "Unused/gameplay PNG missing or eagerly loaded");
        for (const char* invalid : {"", ".", "..", "../tree.png", "/tree.png", "map/../tree.png", "map//tree.png", "map\\tree.png", "map/"})
            test::expect(!content::valid_key(invalid) && !first.path(invalid), "Accepted invalid/aliased asset key");
        test::expect(!first.path("map/missing.png"), "Missing key was silently resolved");
        const auto font = first.font_data("fonts/CozetteVector.ttf");
        test::expect(font && !font->empty() && first.find("fonts/CozetteVector.ttf")->status == content::LoadStatus::loaded,
                     "Font bytes/status unavailable");
        test::expect(!first.font_data("map/tree1.png"), "An image was accepted as a font");
        std::filesystem::remove(temp.path / "fonts/CozetteVector.ttf");
        test::expect(!first.font_data("fonts/CozetteVector.ttf") && first.find("fonts/CozetteVector.ttf")->status == content::LoadStatus::failed &&
                     first.error().find("fonts/CozetteVector.ttf") != std::string::npos, "Missing font diagnostics lack its key");
        temp.write("added/new.PNG", "catalog metadata does not decode images");
        test::expect(!first.find("added/new.PNG"), "Catalog unexpectedly rescanned while browsing");
        content::Catalog updated;
        test::expect(updated.initialize(temp.path) && updated.find("added/new.PNG") &&
                     updated.find("added/new.PNG")->type == content::AssetType::image, "New PNG requires a filename list");
        content::Catalog missing;
        test::expect(!missing.initialize(temp.path / "missing") && !missing.error().empty(), "Missing root did not fail explicitly");
    });
}
