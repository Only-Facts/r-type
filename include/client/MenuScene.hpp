#ifndef MENUSCENE_HPP
#define MENUSCENE_HPP

#include "scene/IScene.hpp"
#include "gfx/IBackground.hpp"
#include "gfx/ITextFactory.hpp"
#include "input/IInputManager.hpp"
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace rtype::engine {
  class MenuScene : public IScene {
  public:
    using StartGameCallback = std::function<void(const std::string& host, unsigned short port)>;

    MenuScene(IInputManager& inputMgr,
              ITextFactory& textFactory,
              std::string defaultHost,
              unsigned short defaultPort,
              StartGameCallback onStartGame,
              std::function<void()> onQuit);
    ~MenuScene() override = default;

    void init() override;
    void update(float dt) override;
    void render(IWindow& window) override;

  private:
    enum class Item { Host, Port, Play, Quit, Count };

    static constexpr std::size_t ITEM_COUNT = static_cast<std::size_t>(Item::Count);

    void moveSelection(int offset);
    void editSelectedField(const std::string& typedText);
    void confirm();
    std::optional<unsigned short> parsePort() const;
    void refreshItemTexts();
    bool isEditable(Item item) const;

    IInputManager& _inputMgr;
    std::unique_ptr<IBackground> _background;
    StartGameCallback _onStartGame;
    std::function<void()> _onQuit;

    std::shared_ptr<IText> _title;
    std::shared_ptr<IText> _subtitle;
    std::array<std::shared_ptr<IText>, ITEM_COUNT> _items;
    std::shared_ptr<IText> _selectionMarker;
    std::shared_ptr<IText> _error;
    std::shared_ptr<IText> _hint;

    std::string _host;
    std::string _port;
    Item _selected{Item::Play};
    float _cursorTimer{0.0f};
  };
}

#endif // !MENUSCENE_HPP
