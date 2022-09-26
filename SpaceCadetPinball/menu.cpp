#include "pch.h"
#include "menu.h"
#include "pb.h"
#include "winmain.h"
#include "options.h"
#include "Sound.h"
#include "midi.h"
#include "fullscrn.h"
#include "render.h"
#include "control.h"
#include "translations.h"

bool menu::ShowWindow = false;

void menu::RenderMenuWindow()
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    if (ImGui::Begin("Options Menu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("GAME");
        if (ImGui::Button(pb::get_rc_string(Msg::Menu1_New_Game)))
            winmain::new_game();
        ImGui::SameLine();

        if (!winmain::LaunchBallEnabled)
            ImGui::BeginDisabled();
        if (ImGui::Button(pb::get_rc_string(Msg::Menu1_Launch_Ball)))
        {
            winmain::end_pause();
            pb::launch_ball();
        }
        if (!winmain::LaunchBallEnabled)
            ImGui::EndDisabled();
        ImGui::SameLine();

        if (!winmain::HighScoresEnabled)
            ImGui::BeginDisabled();
        if (ImGui::Button(pb::get_rc_string(Msg::Menu1_High_Scores)))
        {
            winmain::pause(false);
            pb::high_scores();
        }
        if (!winmain::HighScoresEnabled)
            ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Checkbox(pb::get_rc_string(Msg::Menu1_Demo), &winmain::DemoActive))
        {
            winmain::end_pause();
            pb::toggle_demo();
        }

        ImGui::Separator();
        ImGui::Text("OPTIONS");

        if (ImGui::BeginCombo(pb::get_rc_string(Msg::Menu1_Select_Players), pb::get_rc_string((Msg)((int)Msg::Menu1_1Player + options::Options.Players - 1))))
        {
            for (int i = 0; i < 4; i++)
            {
                if (ImGui::Selectable(pb::get_rc_string((Msg)((int)Msg::Menu1_1Player + i)), i == options::Options.Players - 1))
                {
                    options::toggle((Menu1)((int)Menu1::OnePlayer + i));
                    winmain::new_game();
                }

                if (i == options::Options.Players - 1)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("Language (requires restart)", translations::GetCurrentLanguage()->DisplayName))
        {
            auto currentLanguage = translations::GetCurrentLanguage();
            for (auto &item : translations::Languages)
            {
                if (ImGui::Selectable(item.DisplayName, currentLanguage->Language == item.Language))
                {
                    translations::SetCurrentLanguage(item.ShortName);
                    winmain::Restart();
                }

                if (currentLanguage->Language == item.Language)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Text("AUDIO");
        ImGui::Checkbox(pb::get_rc_string(Msg::Menu1_Sounds), &options::Options.Sounds);
        ImGui::SameLine();
        ImGui::Checkbox("Stereo Sound Effects", &options::Options.SoundStereo);
        ImGui::SameLine();
        if (ImGui::Checkbox(pb::get_rc_string(Msg::Menu1_Music), &options::Options.Music))
        {
            if (!options::Options.Music)
                midi::music_stop();
            else
                midi::music_play();
        }
        ImGui::PushItemWidth(ImGui::GetWindowWidth() / 3 - 10.0f);
        if (ImGui::SliderInt("##Sound Volume", &options::Options.SoundVolume, options::MinVolume, options::MaxVolume, "Sound Volume: %d",
                             ImGuiSliderFlags_AlwaysClamp))
            Sound::SetVolume(options::Options.SoundVolume);
        ImGui::SameLine();
        if (ImGui::SliderInt("##Music Volume", &options::Options.MusicVolume, options::MinVolume, options::MaxVolume, "Music Volume: %d",
                             ImGuiSliderFlags_AlwaysClamp))
            midi::SetVolume(options::Options.MusicVolume);
        ImGui::SameLine();
        if (ImGui::SliderInt("##Sound Channels", &options::Options.SoundChannels, options::MinSoundChannels,
                             options::MaxSoundChannels, "Sound Channels: %d", ImGuiSliderFlags_AlwaysClamp))
            Sound::SetChannels(options::Options.SoundChannels);
        ImGui::PopItemWidth();

        ImGui::Text("GRAPHICS");

        if (ImGui::Checkbox(pb::get_rc_string(Msg::Menu1_WindowUniformScale), &options::Options.UniformScaling))
            fullscrn::window_size_changed();
        ImGui::SameLine();
        if (ImGui::Checkbox("Linear Filtering", &options::Options.LinearFiltering))
            render::recreate_screen_texture();
        ImGui::SameLine();
        if (ImGui::Checkbox("Integer Scaling", &options::Options.IntegerScaling))
            fullscrn::window_size_changed();

        ImGui::Text("GAME DATA");
        if (ImGui::Checkbox("Prefer 3DPB Data (requires restart)", &options::Options.Prefer3DPBGameData))
            winmain::Restart();

        // these are kinda pointless on the wii u
        // ImGui::Text("OTHER");
        // if (ImGui::Button("Sprite Viewer"))
        //     winmain::ShowSpriteViewer = true;
        // ImGui::SameLine();
        // ImGui::Checkbox("Debug Overlay", &options::Options.DebugOverlay);

        ImGui::Text("CHEATS");
        ImGui::Checkbox("hidden test", &pb::cheat_mode);
        ImGui::SameLine();
        ImGui::Checkbox("bmax", &control::table_unlimited_balls);
        ImGui::SameLine();
        if (ImGui::Button("1max"))
            pb::PushCheat("1max");
        ImGui::SameLine();
        if (ImGui::Button("gmax"))
            pb::PushCheat("gmax");
        ImGui::SameLine();
        if (ImGui::Button("rmax"))
            pb::PushCheat("rmax");
        ImGui::SameLine();
        if (pb::FullTiltMode && ImGui::Button("quote"))
            pb::PushCheat("quote");

        // put the close button at the bottom
        ImGui::BeginChild("padding", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        ImGui::EndChild();

        if (ImGui::Button("Close"))
            ShowWindow = false;
        ImGui::SameLine();
        if (ImGui::Button(pb::get_rc_string(Msg::Menu1_About_Pinball)))
        {
            winmain::pause(false);
            winmain::ShowAboutDialog = true;
        }
    }
    ImGui::End();
}