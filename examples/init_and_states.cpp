constexpr ivec2 screen_size = ivec2(480, 270);
constexpr std::string_view window_name = "Iota";

Interface::Window window(std::string(window_name), screen_size * 2, Interface::windowed, adjust_(Interface::WindowSettings{}, min_size = screen_size));
Graphics::DummyVertexArray dummy_vao = nullptr;

Audio::Context audio_context = nullptr;
Audio::SourceManager audio_controller;

const Graphics::ShaderConfig shader_config = Graphics::ShaderConfig::Core();
Interface::ImGuiController gui_controller(Poly::derived<Interface::ImGuiController::GraphicsBackend_Modern>, adjust_(Interface::ImGuiController::Config{}, shader_header = shader_config.common_header));

namespace Fonts
{
    namespace Files
    {
        Graphics::FontFile main("assets/Monocat_6x12.ttf", 12);
    }

    Graphics::Font main;
}

Graphics::TextureAtlas texture_atlas = []{
    Graphics::TextureAtlas ret(ivec2(2048), "assets/_images", "assets/atlas.png", "assets/atlas.refl", {{"/font_storage", ivec2(256)}});
    auto font_region = ret.Get("/font_storage");

    Unicode::CharSet glyph_ranges;
    glyph_ranges.Add(Unicode::Ranges::Basic_Latin);

    Graphics::MakeFontAtlas(ret.GetImage(), font_region.pos, font_region.size, {
        {Fonts::main, Fonts::Files::main, glyph_ranges, Graphics::FontFile::monochrome_with_hinting},
    });
    return ret;
}();
Graphics::Texture texture_main = Graphics::Texture(nullptr).Wrap(Graphics::clamp).Interpolation(Graphics::nearest).SetData(texture_atlas.GetImage());

GameUtils::AdaptiveViewport adaptive_viewport(shader_config, screen_size);
Render r = adjust_(Render(0x2000, shader_config), SetTexture(texture_main), SetMatrix(adaptive_viewport.GetDetails().MatrixCentered()));

Input::Mouse mouse;

STRUCT( StateBase EXTENDS GameUtils::State::Base POLYMORPHIC )
{
    virtual void Render() const = 0;
};

struct Application : Program::DefaultBasicState
{
    GameUtils::State::Manager<StateBase> state_manager;
    GameUtils::FpsCounter fps_counter;

    void Resize()
    {
        adaptive_viewport.Update();
        mouse.SetMatrix(adaptive_viewport.GetDetails().MouseMatrixCentered());
    }

    Metronome metronome = Metronome(60);

    Metronome *GetTickMetronome() override
    {
        return &metronome;
    }

    int GetFpsCap() override
    {
        return 60 * NeedFpsCap();
    }

    void EndFrame() override
    {
        fps_counter.Update();
        window.SetTitle(STR((window_name), " TPS:", (fps_counter.Tps()), " FPS:", (fps_counter.Fps())));
    }

    void Tick() override
    {
        // window.ProcessEvents();
        window.ProcessEvents({gui_controller.EventHook()});

        if (window.ExitRequested())
            Program::Exit();
        if (window.Resized())
        {
            Resize();
            Graphics::Viewport(window.Size());
        }

        gui_controller.PreTick();
        state_manager.Tick();
        audio_controller.Tick();

        Audio::CheckErrors();
    }

    void Render() override
    {
        gui_controller.PreRender();
        adaptive_viewport.BeginFrame();
        state_manager.Call(&StateBase::Render);
        adaptive_viewport.FinishFrame();
        gui_controller.PostRender();
        Graphics::CheckErrors();

        window.SwapBuffers();
    }


    void Init()
    {
        ImGui::StyleColorsDark();

        // Load various small fonts
        auto monochrome_font_flags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;

        gui_controller.LoadFont("assets/Monocat_6x12.ttf", 12.0f, adjust(ImFontConfig{}, FontBuilderFlags = monochrome_font_flags));
        gui_controller.LoadDefaultFont();
        gui_controller.RenderFontsWithFreetype();

        Graphics::Blending::Enable();
        Graphics::Blending::FuncNormalPre();

        state_manager.SetState("Initial{}");
    }
};

namespace States
{
    STRUCT( Initial EXTENDS StateBase )
    {
        MEMBERS()

        float angle = 0;

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            angle += 0.01;
            ImGui::ShowDemoWindow();

            if (mouse.right.pressed())
            {
                static Audio::Buffer buf = []{
                    int16_t array[10000];
                    for (size_t i = 0; i < std::size(array); i++)
                        array[i] = std::sin(i / 30.f) * 0x7fff;
                    return Audio::Sound(48000, Audio::mono, std::size(array), array);
                }();
                audio_controller.Add(buf)->play();
            }
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(mouse.pos(), ivec2(32)).center().rotate(angle).color(mouse.left.down() ? fvec3(1,0.5,0) : fvec3(0,0.5,1));
            r.itext(mouse.pos(), Graphics::Text(Fonts::main, STR((audio_controller.ActiveSources())))).color(fvec3(1));

            r.Finish();
        }
    };
}

IMP_MAIN(,)
{
    Application app;
    app.Init();
    app.Resize();
    app.RunMainLoop();
    return 0;
}
