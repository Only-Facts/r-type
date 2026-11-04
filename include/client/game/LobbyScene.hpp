#ifndef RTYPE_GAME_LOBBYSCENE_HPP
#define RTYPE_GAME_LOBBYSCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include "engine/scene/AScene.hpp"
#include "game/Difficulty.hpp"
#include "network/ClientNetworkManager.hpp"
#include "network/INetworkSocket.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rtype::game {
  class LobbyScene : public engine::AScene {
  public:
    static constexpr const char* SESSION_ROOM = "room";

    explicit LobbyScene(engine::EngineContext& context);
    ~LobbyScene() override;

    void exit() override;

  protected:
    void onInit() override;
    void onUpdate(float dt) override;

  private:
    using RoomInfo = client::ClientNetworkManager::RoomInfo;
    using RoomResult = client::ClientNetworkManager::RoomResult;

    enum class Mode { Browse, Create, Creating };

    engine::Entity addText(const std::string& text, float anchorY, float offsetY, unsigned int size,
                           const engine::Color& color);
    void requestRooms();
    void onRoomList(RoomResult result, const std::vector<RoomInfo>& rooms);
    void moveSelection(int step);
    void joinSelected();
    void joinRoom(std::uint32_t roomId);
    void setMode(Mode mode);
    void updateCreatePanel();
    void confirmCreate();
    void onRoomCreated(RoomResult result, std::uint32_t roomId);
    void refreshPanel();
    void refreshRows();
    void setStatus(const std::string& key, const std::string& fallback, const engine::Color& color);
    std::string roomLine(const RoomInfo& room) const;
    void playConfigSound(const std::string& key);

    std::unique_ptr<network::INetworkSocket> _socket;
    client::ClientNetworkManager _network;
    Difficulty _difficulty;
    std::vector<RoomInfo> _rooms;
    std::vector<engine::Entity> _rows;
    engine::Entity _status{0};
    engine::Entity _panel{0};
    engine::Entity _browseHint{0};
    engine::Entity _createHint{0};
    Mode _mode{Mode::Browse};
    int _capacity;
    std::size_t _createDifficulty{0};
    std::size_t _selected{0};
    std::optional<std::uint32_t> _selectedId;
    bool _waitingList{false};
    float _refreshTimer{0.0f};

    std::string _host;
    unsigned short _port;
    std::string _font;
    std::string _upAction;
    std::string _downAction;
    std::string _startAction;
    std::string _backAction;
    std::string _createAction;
    std::string _prevAction;
    std::string _nextAction;
    std::string _nextScene;
    std::string _previousScene;
    int _maxCapacity;
    float _refreshInterval;
    float _outlineThickness;
    engine::Color _textColor;
    engine::Color _selectedColor;
    engine::Color _fullColor;
    engine::Color _dimColor;
    engine::Color _errorColor;
  };
}

#endif // !RTYPE_GAME_LOBBYSCENE_HPP
