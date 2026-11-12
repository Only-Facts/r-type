#include "game/LobbyScene.hpp"
#include "engine/audio/AudioService.hpp"
#include "engine/config/Config.hpp"
#include "engine/core/TextTemplate.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/input/ActionMap.hpp"
#include "engine/platform/IBackend.hpp"
#include "engine/platform/IWindow.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/scene/SceneManager.hpp"
#include "engine/systems/MovementSystem.hpp"
#include "engine/systems/ParallaxSystem.hpp"
#include "exception.hpp"
#include "game/StarfieldSpawner.hpp"
#include <algorithm>

namespace rtype::game {
  namespace {
    constexpr const char* SECTION = "lobby";
    constexpr const char* SECTION_NETWORK = "network";
    constexpr std::uint8_t ROOM_STATE_FULL = 2;
    constexpr int PROTOCOL_MAX_CAPACITY = 4;

    unsigned int readSize(const engine::Config& config, const std::string& key, int fallback) {
      const int size = config.getInt(SECTION, key, fallback);
      if (size <= 0) throw RtypeError(std::string("[lobby] ") + key + " must be > 0");
      return static_cast<unsigned int>(size);
    }

    unsigned short readPort(const engine::Config& config) {
      const int port = config.getInt(SECTION_NETWORK, "port", 4242);
      if (port <= 0 || port > 65535) throw RtypeError("[network] port must be in [1, 65535]");
      return static_cast<unsigned short>(port);
    }
  }

  LobbyScene::LobbyScene(engine::EngineContext& context)
    : AScene(context),
      _socket(context.backend.createUdpSocket()),
      _network(*_socket, _registry),
      _host(context.config.getString(SECTION_NETWORK, "host", "127.0.0.1")),
      _port(readPort(context.config)),
      _font(context.config.getString(SECTION, "font", "font_hud")),
      _upAction(context.config.getString(SECTION, "up_action", "menu_up")),
      _downAction(context.config.getString(SECTION, "down_action", "menu_down")),
      _startAction(context.config.getString(SECTION, "start_action", "start")),
      _backAction(context.config.getString(SECTION, "back_action", "back")),
      _createAction(context.config.getString(SECTION, "create_action", "menu_create")),
      _prevAction(context.config.getString(SECTION, "prev_action", "menu_prev")),
      _nextAction(context.config.getString(SECTION, "next_action", "menu_next")),
      _nextScene(context.config.getString(SECTION, "next_scene", "game")),
      _previousScene(context.config.getString(SECTION, "previous_scene", "skin")),
      _maxCapacity(std::clamp(context.config.getInt(SECTION, "max_capacity", PROTOCOL_MAX_CAPACITY), 1,
                              PROTOCOL_MAX_CAPACITY)),
      _refreshInterval(context.config.getFloat(SECTION, "refresh_interval", 2.0f)),
      _outlineThickness(context.config.getFloat(SECTION, "outline_thickness", 2.0f)),
      _textColor(context.config.getColor(SECTION, "text_color", engine::Color{255, 255, 255, 255})),
      _selectedColor(context.config.getColor(SECTION, "selected_color", engine::Color{120, 200, 255, 255})),
      _fullColor(context.config.getColor(SECTION, "full_color", engine::Color{150, 150, 160, 255})),
      _dimColor(context.config.getColor(SECTION, "dim_color", engine::Color{170, 170, 190, 255})),
      _errorColor(context.config.getColor(SECTION, "error_color", engine::Color{255, 120, 120, 255})) {
    if (_refreshInterval <= 0.0f) throw RtypeError("[lobby] refresh_interval must be > 0");
    _difficulty.load(context.config, "difficulty");
    _capacity = _maxCapacity;
    for (const char* key : {"move_sound", "confirm_sound", "back_sound", "error_sound"}) {
      const std::string sound = context.config.getString(SECTION, key);
      if (!sound.empty()) context.resources.get(sound, engine::AssetType::Sound);
    }
    _network.setOnRoomList([this](RoomResult result, const std::vector<RoomInfo>& rooms) {
      onRoomList(result, rooms);
    });
    _network.setOnRoomCreated([this](RoomResult result, std::uint32_t roomId) { onRoomCreated(result, roomId); });
  }

  LobbyScene::~LobbyScene() {
    _network.disconnect();
  }

  void LobbyScene::exit() {
    _network.disconnect();
  }

  void LobbyScene::onInit() {
    const engine::Config& cfg = _context.config;

    addSystem<engine::MovementSystem>();
    addSystem<engine::ParallaxSystem>(_context.window);
    StarfieldSpawner starfield(_registry, _factory);
    starfield.spawn(StarfieldSpawner::loadSettings(cfg, "starfield"), _context.window.getSize());

    addText(cfg.getString(SECTION, "title_text", "Choose a room"), cfg.getFloat(SECTION, "title_y", 0.15f), 0.0f,
            readSize(cfg, "title_size", 32), cfg.getColor(SECTION, "title_color", _textColor));
    _status = addText("", cfg.getFloat(SECTION, "status_y", 0.25f), 0.0f, readSize(cfg, "status_size", 14),
                      _dimColor);
    _error = addText("", cfg.getFloat(SECTION, "error_y", 0.29f), 0.0f, readSize(cfg, "error_size", 14),
                     _errorColor);

    const float listY = cfg.getFloat(SECTION, "list_y", 0.33f);
    const unsigned int rowSize = readSize(cfg, "row_size", 18);
    const float rowHeight = static_cast<float>(rowSize) * cfg.getFloat(SECTION, "row_spacing", 2.0f);
    const int rowCount = std::max(1, cfg.getInt(SECTION, "max_rows", 8));
    for (int i = 0; i < rowCount; ++i)
      _rows.push_back(addText("", listY, rowHeight * static_cast<float>(i), rowSize, _textColor));

    const engine::TemplateParams keys{
      {"up", _context.actions.describe(_upAction)},
      {"down", _context.actions.describe(_downAction)},
      {"prev", _context.actions.describe(_prevAction)},
      {"next", _context.actions.describe(_nextAction)},
      {"start", _context.actions.describe(_startAction)},
      {"back", _context.actions.describe(_backAction)},
      {"create", _context.actions.describe(_createAction)}};
    const float hintY = cfg.getFloat(SECTION, "hint_y", 0.88f);
    const unsigned int hintSize = readSize(cfg, "hint_size", 12);
    _browseHint = addText(engine::formatTemplate(cfg.getString(SECTION, "hint_text",
                            "{up} - {down} to choose   {start} to join   {create} to create   {back} to go back"), keys),
                          hintY, 0.0f, hintSize, _dimColor);
    _createHint = addText(engine::formatTemplate(cfg.getString(SECTION, "create_hint_text",
                            "{prev} - {next} players   {up} - {down} difficulty   {start} to create   {back} to cancel"),
                            keys),
                          hintY, 0.0f, hintSize, _dimColor);
    _panel = addText("", cfg.getFloat(SECTION, "panel_y", 0.78f), 0.0f, readSize(cfg, "panel_size", 20),
                     _selectedColor);
    _createDifficulty = _difficulty.selected(_context.session);
    setMode(Mode::Browse);
    showJoinError();

    setStatus("connecting_text", "Connecting to {host}:{port}...", _dimColor);
    _network.openLobby(_host, _port);
    requestRooms();
    refreshRows();
  }

  void LobbyScene::onUpdate(float dt) {
    _network.update(dt);

    _refreshTimer += std::max(dt, 0.0f);
    if (!_waitingList && _refreshTimer >= _refreshInterval) requestRooms();

    const engine::ActionMap& actions = _context.actions;
    if (_mode == Mode::Create) {
      updateCreatePanel();
      return;
    }
    if (_mode == Mode::Creating) return;
    if (actions.isJustPressed(_createAction)) {
      playConfigSound("move_sound");
      clearError();
      setMode(Mode::Create);
      return;
    }
    if (actions.isJustPressed(_upAction)) moveSelection(-1);
    if (actions.isJustPressed(_downAction)) moveSelection(1);
    if (actions.isJustPressed(_startAction)) {
      joinSelected();
    } else if (actions.isJustPressed(_backAction)) {
      playConfigSound("back_sound");
      _context.scenes.changeScene(_previousScene);
    }
  }

  void LobbyScene::requestRooms() {
    _refreshTimer = 0.0f;
    _waitingList = _network.requestRooms();
  }

  void LobbyScene::onRoomList(RoomResult result, const std::vector<RoomInfo>& rooms) {
    _waitingList = false;
    _refreshTimer = 0.0f;
    if (result != RoomResult::Success) {
      setStatus("no_answer_text", "No answer from {host}:{port}", _errorColor);
      return;
    }
    _rooms = rooms;
    _selected = 0;
    for (std::size_t i = 0; i < _rooms.size(); ++i)
      if (_selectedId && _rooms[i].id == *_selectedId) _selected = i;
    _selectedId = _rooms.empty() ? std::nullopt : std::optional<std::uint32_t>{_rooms[_selected].id};
    if (_rooms.empty()) setStatus("empty_text", "No room on {host}:{port}", _dimColor);
    else setStatus("rooms_text", "{count} rooms on {host}:{port}", _dimColor);
    refreshRows();
  }

  void LobbyScene::moveSelection(int step) {
    if (_rooms.size() <= 1) return;
    const auto count = static_cast<int>(_rooms.size());
    _selected = static_cast<std::size_t>(((static_cast<int>(_selected) + step) % count + count) % count);
    _selectedId = _rooms[_selected].id;
    playConfigSound("move_sound");
    clearError();
    refreshRows();
  }

  void LobbyScene::joinSelected() {
    if (_rooms.empty()) {
      playConfigSound("error_sound");
      return;
    }
    if (_rooms[_selected].state == ROOM_STATE_FULL) {
      playConfigSound("error_sound");
      showError("error_full_text", "Room {room} is full", _rooms[_selected].id);
      return;
    }
    playConfigSound("confirm_sound");
    joinRoom(_rooms[_selected].id);
  }

  void LobbyScene::joinRoom(std::uint32_t roomId) {
    _context.session.setInt(SESSION_ROOM, static_cast<int>(roomId));
    _context.session.setInt(SESSION_ROOM_SEEN, 1);
    _context.scenes.changeScene(_nextScene);
  }

  void LobbyScene::setMode(Mode mode) {
    _mode = mode;
    if (auto* hint = _registry.getComponent<engine::TextComponent>(_browseHint)) hint->visible = mode == Mode::Browse;
    if (auto* hint = _registry.getComponent<engine::TextComponent>(_createHint)) hint->visible = mode == Mode::Create;
    if (auto* panel = _registry.getComponent<engine::TextComponent>(_panel)) panel->visible = mode != Mode::Browse;
    refreshPanel();
  }

  void LobbyScene::updateCreatePanel() {
    const engine::ActionMap& actions = _context.actions;
    const int levels = static_cast<int>(_difficulty.count());
    int capacityStep = 0;
    int difficultyStep = 0;
    if (actions.isJustPressed(_prevAction)) capacityStep = -1;
    if (actions.isJustPressed(_nextAction)) capacityStep = 1;
    if (actions.isJustPressed(_upAction)) difficultyStep = -1;
    if (actions.isJustPressed(_downAction)) difficultyStep = 1;
    if (capacityStep != 0) _capacity = std::clamp(_capacity + capacityStep, 1, _maxCapacity);
    if (difficultyStep != 0 && levels > 0) {
      const int current = static_cast<int>(_createDifficulty);
      _createDifficulty = static_cast<std::size_t>(((current + difficultyStep) % levels + levels) % levels);
    }
    if (capacityStep != 0 || difficultyStep != 0) {
      playConfigSound("move_sound");
      refreshPanel();
    }

    if (actions.isJustPressed(_startAction)) {
      confirmCreate();
    } else if (actions.isJustPressed(_backAction)) {
      playConfigSound("back_sound");
      setMode(Mode::Browse);
    }
  }

  void LobbyScene::confirmCreate() {
    if (!_network.createRoom(static_cast<std::uint8_t>(_capacity), static_cast<std::uint8_t>(_createDifficulty))) {
      playConfigSound("error_sound");
      return;
    }
    playConfigSound("confirm_sound");
    clearError();
    setMode(Mode::Creating);
    setStatus("creating_text", "Creating the room...", _dimColor);
  }

  void LobbyScene::onRoomCreated(RoomResult result, std::uint32_t roomId) {
    if (result == RoomResult::Success) {
      joinRoom(roomId);
      return;
    }
    playConfigSound("error_sound");
    setMode(Mode::Create);
    if (result == RoomResult::LimitReached)
      showError("limit_text", "The server cannot host more rooms", 0);
    else if (result == RoomResult::TimedOut)
      showError("no_answer_text", "No answer from {host}:{port}", 0);
    else
      showError("create_failed_text", "The server refused this room", 0);
  }

  void LobbyScene::showJoinError() {
    engine::Session& session = _context.session;
    const int code = session.getInt(SESSION_ERROR, -1);
    if (code < 0) return;
    const auto roomId = static_cast<std::uint32_t>(std::max(0, session.getInt(SESSION_ERROR_ROOM, 0)));
    const bool seen = session.getInt(SESSION_ROOM_SEEN, 0) == 1;
    session.setInt(SESSION_ERROR, -1);
    session.setInt(SESSION_ROOM_SEEN, 0);

    switch (static_cast<RoomResult>(code)) {
      case RoomResult::Full:
        showError("error_full_text", "Room {room} is full", roomId);
        break;
      case RoomResult::NotFound:
        if (seen) showError("error_expired_text", "Room {room} has expired", roomId);
        else showError("error_not_found_text", "Room {room} does not exist", roomId);
        break;
      case RoomResult::AlreadyInRoom:
        showError("error_busy_text", "Still leaving the previous room, try again", roomId);
        break;
      case RoomResult::TimedOut:
        showError("no_answer_text", "No answer from {host}:{port}", roomId);
        break;
      default:
        showError("error_join_text", "Could not join room {room}", roomId);
        break;
    }
  }

  void LobbyScene::showError(const std::string& key, const std::string& fallback, std::uint32_t roomId) {
    auto* text = _registry.getComponent<engine::TextComponent>(_error);
    if (!text) return;
    text->text = engine::formatTemplate(_context.config.getString(SECTION, key, fallback),
                                        {{"host", _host}, {"port", std::to_string(_port)},
                                         {"room", std::to_string(roomId)}});
  }

  void LobbyScene::clearError() {
    if (auto* text = _registry.getComponent<engine::TextComponent>(_error)) text->text.clear();
  }

  void LobbyScene::refreshPanel() {
    auto* panel = _registry.getComponent<engine::TextComponent>(_panel);
    if (!panel) return;
    const std::string difficulty = _createDifficulty < _difficulty.count()
      ? _difficulty.at(_createDifficulty).label : std::to_string(_createDifficulty);
    const std::string key = _mode == Mode::Creating ? "panel_waiting_text" : "panel_text";
    const std::string fallback = _mode == Mode::Creating
      ? "Creating a room for {capacity} players, {difficulty}..."
      : "New room   < {capacity} players >   < {difficulty} >";
    panel->text = engine::formatTemplate(_context.config.getString(SECTION, key, fallback),
                                         {{"capacity", std::to_string(_capacity)}, {"difficulty", difficulty}});
  }

  void LobbyScene::refreshRows() {
    const std::size_t rowCount = _rows.size();
    std::size_t first = 0;
    if (_rooms.size() > rowCount)
      first = std::min(_selected >= rowCount / 2 ? _selected - rowCount / 2 : 0, _rooms.size() - rowCount);

    for (std::size_t row = 0; row < rowCount; ++row) {
      auto* text = _registry.getComponent<engine::TextComponent>(_rows[row]);
      if (!text) continue;
      const std::size_t index = first + row;
      if (index >= _rooms.size()) {
        text->text.clear();
        continue;
      }
      const RoomInfo& room = _rooms[index];
      const bool selected = index == _selected;
      text->text = (selected ? "> " : "  ") + roomLine(room) + (selected ? " <" : "  ");
      text->color = selected ? _selectedColor : room.state == ROOM_STATE_FULL ? _fullColor : _textColor;
    }
  }

  std::string LobbyScene::roomLine(const RoomInfo& room) const {
    const engine::Config& cfg = _context.config;
    std::string state = cfg.getString(SECTION, "state_waiting", "waiting");
    if (room.state == 1) state = cfg.getString(SECTION, "state_running", "in game");
    else if (room.state == ROOM_STATE_FULL) state = cfg.getString(SECTION, "state_full", "full");
    const std::string difficulty = room.difficulty < _difficulty.count()
      ? _difficulty.at(room.difficulty).label : std::to_string(room.difficulty);
    const int free = std::max(0, static_cast<int>(room.capacity) - static_cast<int>(room.players));
    return engine::formatTemplate(
      cfg.getString(SECTION, "room_text", "Room {id}   {players}/{capacity}   {free} free   {difficulty}   {state}"),
      {{"id", std::to_string(room.id)},
       {"players", std::to_string(room.players)},
       {"capacity", std::to_string(room.capacity)},
       {"free", std::to_string(free)},
       {"difficulty", difficulty},
       {"state", state}});
  }

  void LobbyScene::setStatus(const std::string& key, const std::string& fallback, const engine::Color& color) {
    auto* text = _registry.getComponent<engine::TextComponent>(_status);
    if (!text) return;
    text->text = engine::formatTemplate(_context.config.getString(SECTION, key, fallback),
                                        {{"host", _host}, {"port", std::to_string(_port)},
                                         {"count", std::to_string(_rooms.size())}});
    text->color = color;
  }

  engine::Entity LobbyScene::addText(const std::string& text, float anchorY, float offsetY, unsigned int size,
                                     const engine::Color& color) {
    const engine::Entity entity = _registry.createEntity();
    _registry.addComponent(entity, engine::TransformComponent{});

    engine::ScreenAnchorComponent anchor;
    anchor.anchor = {0.5f, anchorY};
    anchor.offset = {0.0f, offsetY};
    _registry.addComponent(entity, anchor);

    engine::TextComponent component;
    component.fontId = _font;
    component.text = text;
    component.characterSize = size;
    component.color = color;
    component.outlineThickness = _outlineThickness;
    component.origin = {0.5f, 0.0f};
    _registry.addComponent(entity, component);
    return entity;
  }

  void LobbyScene::playConfigSound(const std::string& key) {
    const std::string sound = _context.config.getString(SECTION, key);
    if (!sound.empty()) _context.audio.play(sound);
  }
}
