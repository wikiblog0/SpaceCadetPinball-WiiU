#include "pch.h"
#include "winmain.h"

#include "control.h"
#include "fullscrn.h"
#include "midi.h"
#include "options.h"
#include "pb.h"
#include "render.h"
#include "Sound.h"
#include "translations.h"
#include "font_selection.h"
#include "menu.h"

#include <whb/proc.h>
#include <sys/stat.h>
#include <sysapp/switch.h>
#include <coreinit/debug.h>

#include <locale>
#include <codecvt>

#ifdef USE_ROMFS
#include <romfs-wiiu.h>
#endif

SDL_Window* winmain::MainWindow = nullptr;
SDL_Renderer* winmain::Renderer = nullptr;
ImGuiIO* winmain::ImIO = nullptr;

int winmain::return_value = 0;
bool winmain::bQuit = false;
bool winmain::activated = false;
int winmain::DispFrameRate = 0;
bool winmain::DispGRhistory = false;
bool winmain::single_step = false;
bool winmain::has_focus = true;
int winmain::last_mouse_x;
int winmain::last_mouse_y;
int winmain::mouse_down;
bool winmain::no_time_loss = false;

bool winmain::restart = false;

gdrv_bitmap8* winmain::gfr_display = nullptr;
bool winmain::ShowAboutDialog = false;
bool winmain::ShowImGuiDemo = false;
bool winmain::ShowSpriteViewer = false;
bool winmain::LaunchBallEnabled = true;
bool winmain::HighScoresEnabled = true;
bool winmain::DemoActive = false;
int winmain::MainMenuHeight = 0;
std::string winmain::FpsDetails, winmain::PrevSdlError;
unsigned winmain::PrevSdlErrorCount = 0;
double winmain::UpdateToFrameRatio;
winmain::DurationMs winmain::TargetFrameTime;
optionsStruct& winmain::Options = options::Options;
winmain::DurationMs winmain::SpinThreshold = DurationMs(0.005);
WelfordState winmain::SleepState{};

bool leftTrigger = false;
bool rightTrigger = false;

int winmain::WinMain(LPCSTR lpCmdLine, char16_t* errorMessage)
{
	restart = false;
	bQuit = false;

	std::set_new_handler(memalloc_failure);

	pb::quickFlag = strstr(lpCmdLine, "-quick") != nullptr;

	// SDL window
	SDL_Window* window = SDL_CreateWindow
	(
		pb::get_rc_string(Msg::STRING139),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1920, 1080,
		SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
	);
	MainWindow = window;
	if (!window)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Could not create window", SDL_GetError(), nullptr);
		return 1;
	}

	// If HW fails, fallback to SW SDL renderer.
	SDL_Renderer* renderer = nullptr;
	auto swOffset = strstr(lpCmdLine, "-sw") != nullptr ? 1 : 0;
	for (int i = swOffset; i < 2 && !renderer; i++)
	{
		Renderer = renderer = SDL_CreateRenderer
		(
			window,
			-1,
			i == 0 ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE
		);
	}
	if (!renderer)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Could not create renderer", SDL_GetError(), window);
		return 1;
	}
	SDL_RendererInfo rendererInfo{};
	if (!SDL_GetRendererInfo(renderer, &rendererInfo))
		printf("Using SDL renderer: %s\n", rendererInfo.name);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

	// ImGui init
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWiiU_Init();
	ImGui_ImplSDLRenderer_Init(renderer);
	// Initialize backend stuff
	ImGui_ImplSDLRenderer_NewFrame();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	ImIO = &io;
	io.DisplaySize.x = 1920;
	io.DisplaySize.y = 1080;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = "fs:/vol/external01/wiiu/apps/spacecadetpinball/imgui_pb.ini";

	mkdir("fs:/vol/external01/wiiu", 777);
	mkdir("fs:/vol/external01/wiiu/apps", 777);
	mkdir("fs:/vol/external01/wiiu/apps/spacecadetpinball", 777);

	// First step: just load the options
	options::InitPrimary();

	if (!Options.FontFileName.empty())
	{
		ImGui_ImplSDLRenderer_DestroyFontsTexture();
		io.Fonts->Clear();
		ImVector<ImWchar> ranges;
		translations::GetGlyphRange(&ranges);
		ImFontConfig fontConfig{};

		// ToDo: further tweak font options, maybe try imgui_freetype
		fontConfig.OversampleV = 2;
		fontConfig.OversampleH = 4;

		// ToDo: improve font file test, checking if file exists is not enough
		auto fileName = Options.FontFileName.c_str();
		auto fileHandle = fopenu(fileName, "rb");
		if (fileHandle)
		{
			fclose(fileHandle);

			// ToDo: Bind font size to UI scale
			if (!io.Fonts->AddFontFromFileTTF(fileName, 13.f, &fontConfig, ranges.Data))
				io.Fonts->AddFontDefault();
		}
		else
			io.Fonts->AddFontDefault();

		io.Fonts->Build();
		ImGui_ImplSDLRenderer_CreateFontsTexture();
	}

	// Data search order: WD, executable path, user pref path, platform specific paths.
	std::vector<const char*> searchPaths
	{
		{
			"fs:/vol/external01/wiiu/apps/spacecadetpinball/data/",
			"fs:/vol/content/",
#ifdef USE_ROMFS
			"romfs:/"
#endif
		}
	};
	pb::SelectDatFile(searchPaths);

	// Second step: run updates depending on FullTiltMode
	options::InitSecondary();

	if (!Sound::Init(Options.SoundChannels, Options.Sounds, Options.SoundVolume))
		Options.Sounds = false;

	if (!pb::quickFlag && !midi::music_init(Options.MusicVolume))
		Options.Music = false;

	if (pb::init())
	{
		std::u16string message = u"The .dat file is missing.\n\n"
			"Make sure that the game data is present in any of the following locations:";
		std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert; 
		for (auto path : searchPaths)
		{
			if (path)
			{
				std::u16string u16Path = convert.from_bytes((path[0] ? path : "working directory"));
				// make the sd card path user-friendly
				if (u16Path.find(u"fs:/vol/external01/") != std::string::npos) u16Path.replace(u16Path.find(u"fs:/vol/external01/"), (sizeof(u"fs:/vol/external01/") - 1) / 2, u"sd:/");
				message = message + u"\n" + u16Path;
			}
		}

		// probably a bad idea but it works
		const char16_t* data = message.c_str();
		memcpy(errorMessage, data, message.length() * 2);
		
		// deinit whatever we initted before this
		Sound::Close();
		ImGui_ImplSDLRenderer_Shutdown();
		ImGui_ImplWiiU_Shutdown();
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		ImGui::DestroyContext();
		return 1;
	}

	fullscrn::init();

	pb::reset_table();
	pb::firsttime_setup();

	if (strstr(lpCmdLine, "-fullscreen"))
	{
		Options.FullScreen = true;
	}

	SDL_ShowWindow(window);
	fullscrn::set_screen_mode(Options.FullScreen);

	if (strstr(lpCmdLine, "-demo"))
		pb::toggle_demo();
	else
		pb::replay_level(false);

	unsigned updateCounter = 0, frameCounter = 0;

	auto frameStart = Clock::now();
	double UpdateToFrameCounter = 0;
	DurationMs sleepRemainder(0), frameDuration(TargetFrameTime);
	auto prevTime = frameStart;
	while (true)
	{
		if (DispFrameRate)
		{
			auto curTime = Clock::now();
			if (curTime - prevTime > DurationMs(1000))
			{
				char buf[60];
				auto elapsedSec = DurationMs(curTime - prevTime).count() * 0.001;
				snprintf(buf, sizeof buf, "Updates/sec = %02.02f Frames/sec = %02.02f ",
				         updateCounter / elapsedSec, frameCounter / elapsedSec);
				WHBLogPrintf("%s", buf);
				FpsDetails = buf;
				frameCounter = updateCounter = 0;
				prevTime = curTime;
			}
		}

		if (!ProcessWindowMessages() || bQuit || !WHBProcIsRunning())
			break;

		if (has_focus)
		{
			if (pb::cheat_mode && ImIO->MouseDown[ImGuiMouseButton_Left])
			{
				pb::ballset((-ImIO->MouseDelta.x) / (1920 * 2), ImIO->MouseDelta.y / (1080 * 2));
			}
			if (!single_step && !no_time_loss)
			{
				auto dt = static_cast<float>(frameDuration.count());
				pb::frame(dt);
				if (DispGRhistory)
				{
					auto width = 300;
					auto height = 64, halfHeight = height / 2;
					if (!gfr_display)
					{
						gfr_display = new gdrv_bitmap8(width, height, false);
						gfr_display->CreateTexture("nearest", SDL_TEXTUREACCESS_STREAMING);
					}

					gdrv::ScrollBitmapHorizontal(gfr_display, -1);
					gdrv::fill_bitmap(gfr_display, 1, halfHeight, width - 1, 0, ColorRgba::Black()); // Background
					// Target	
					gdrv::fill_bitmap(gfr_display, 1, halfHeight, width - 1, halfHeight, ColorRgba::White());

					auto target = static_cast<float>(TargetFrameTime.count());
					auto scale = halfHeight / target;
					auto diffHeight = std::min(static_cast<int>(std::round(std::abs(target - dt) * scale)), halfHeight);
					auto yOffset = dt < target ? halfHeight : halfHeight - diffHeight;
					gdrv::fill_bitmap(gfr_display, 1, diffHeight, width - 1, yOffset, ColorRgba::Red()); // Target diff
				}
				updateCounter++;
			}
			no_time_loss = false;

			if (UpdateToFrameCounter >= UpdateToFrameRatio)
			{
				// update input
				ImGui_ImplWiiU_ControllerInput input;
				VPADStatus vpadStatus;
				VPADReadError vpadError;
				KPADStatus kpadStatus[4];
				VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);
				
				if (vpadError == VPAD_READ_SUCCESS) {
					input.vpad = &vpadStatus;
					if (!ImIO->WantCaptureKeyboard) {
						pb::InputDown({InputTypes::Gamepad, vpadStatus.trigger});
						pb::InputUp({InputTypes::Gamepad, vpadStatus.release});
					}
				}

				for (int i = 0; i < 4; i++) {
					KPADError kpadError;
					KPADReadEx((KPADChan)i, &kpadStatus[i], 1, &kpadError);

					if (kpadError == KPAD_ERROR_OK) {
						input.kpad[i] = &kpadStatus[i];

						if (!ImIO->WantCaptureKeyboard) {
							switch (kpadStatus[i].extensionType) {
								case WPAD_EXT_CORE:
								case WPAD_EXT_MPLUS:
									pb::InputDown({InputTypes::Wiimote, kpadStatus[i].trigger});
									pb::InputUp({InputTypes::Wiimote, kpadStatus[i].release});
									break;
								case WPAD_EXT_CLASSIC:
								case WPAD_EXT_MPLUS_CLASSIC:
									pb::InputDown({InputTypes::Classic, kpadStatus[i].classic.trigger});
									pb::InputUp({InputTypes::Classic, kpadStatus[i].classic.release});
									break;
								case WPAD_EXT_PRO_CONTROLLER:
									pb::InputDown({InputTypes::Classic, kpadStatus[i].pro.trigger});
									pb::InputUp({InputTypes::Classic, kpadStatus[i].pro.release});
									break;
							}
						}
					}
				}

				ImGui_ImplWiiU_ProcessInput(&input);
				ImGui_ImplSDLRenderer_NewFrame();
				ImGui::NewFrame();
				RenderUi();

				SDL_RenderClear(renderer);
				// Alternative clear hack, clear might fail on some systems
				// Todo: remove original clear, if save for all platforms
				SDL_RenderFillRect(renderer, nullptr);
				render::PresentVScreen();

				ImGui::Render();
				ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

				SDL_RenderPresent(renderer);
				frameCounter++;
				UpdateToFrameCounter -= UpdateToFrameRatio;
			}

			auto sdlError = SDL_GetError();
			if (sdlError[0] || !PrevSdlError.empty())
			{
				if (sdlError[0])
					SDL_ClearError();

				// Rate limit duplicate SDL error messages.
				if (sdlError != PrevSdlError)
				{
					PrevSdlError = sdlError;
					if (PrevSdlErrorCount > 0)
					{
						printf("SDL Error: ^ Previous Error Repeated %u Times\n", PrevSdlErrorCount + 1);
						PrevSdlErrorCount = 0;
					}

					if (sdlError[0])
						printf("SDL Error: %s\n", sdlError);
				}
				else
				{
					PrevSdlErrorCount++;
				}
			}

			auto updateEnd = Clock::now();
			auto targetTimeDelta = TargetFrameTime - DurationMs(updateEnd - frameStart) - sleepRemainder;

			TimePoint frameEnd;
			if (targetTimeDelta > DurationMs::zero() && !Options.UncappedUpdatesPerSecond)
			{
				if (Options.HybridSleep)
					HybridSleep(targetTimeDelta);
				else
					std::this_thread::sleep_for(targetTimeDelta);
				frameEnd = Clock::now();
			}
			else
			{
				frameEnd = updateEnd;
			}

			// Limit duration to 2 * target time
			sleepRemainder = Clamp(DurationMs(frameEnd - updateEnd) - targetTimeDelta, -TargetFrameTime,
			                       TargetFrameTime);
			frameDuration = std::min<DurationMs>(DurationMs(frameEnd - frameStart), 2 * TargetFrameTime);
			frameStart = frameEnd;
			UpdateToFrameCounter++;
		}
	}

	if (PrevSdlErrorCount > 0)
	{
		printf("SDL Error: ^ Previous Error Repeated %u Times\n", PrevSdlErrorCount);
	}

	delete gfr_display;
	gfr_display = nullptr;
	options::uninit();
	midi::music_shutdown();
	pb::uninit();
	Sound::Close();
	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplWiiU_Shutdown();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	ImGui::DestroyContext();

	return return_value;
}

void winmain::RenderUi()
{
	// A minimal window with a button to prevent menu lockout.
	if (!Options.ShowMenu)
	{
		ImGui::SetNextWindowPos(ImVec2{});
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10, 0});
		if (ImGui::Begin("main", nullptr,
		                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
		                 ImGuiWindowFlags_AlwaysAutoResize |
		                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing))
		{
			ImGui::PushID(1);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{});
			if (ImGui::Button("Menu"))
			{
				options::toggle(Menu1::Show_Menu);
			}
			ImGui::PopStyleColor(1);
			ImGui::PopID();
		}
		ImGui::End();
		ImGui::PopStyleVar();

		// This window can not loose nav focus for some reason, clear it manually.
		if (ImGui::IsNavInputDown(ImGuiNavInput_Cancel))
			ImGui::FocusWindow(nullptr);
	}

	// No demo window in release to save space
#ifndef NDEBUG
	if (ShowImGuiDemo)
		ImGui::ShowDemoWindow(&ShowImGuiDemo);
#endif

	if (menu::ShowWindow)
		menu::RenderMenuWindow();
	a_dialog();
	high_score::RenderHighScoreDialog();
	font_selection::RenderDialog();
	if (ShowSpriteViewer)
		render::SpriteViewer(&ShowSpriteViewer);
	options::RenderControlDialog();
	if (DispGRhistory)
		RenderFrameTimeDialog();

	// Print game texts on the sidebar
	gdrv::grtext_draw_ttext_in_box();
}

int winmain::event_handler(const SDL_Event* event)
{
	if (ImIO->WantCaptureMouse && !options::WaitingForInput())
	{
		if (mouse_down)
		{
			mouse_down = 0;
			SDL_SetWindowGrab(MainWindow, SDL_FALSE);
		}
		switch (event->type)
		{
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEWHEEL:
			return 1;
		default: ;
		}
	}
	if (ImIO->WantCaptureKeyboard && !options::WaitingForInput())
	{
		switch (event->type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			return 1;
		default: ;
		}
	}

	switch (event->type)
	{
	case SDL_QUIT:
		end_pause();
		bQuit = true;
		fullscrn::shutdown();
		return_value = 0;
		return 0;
	case SDL_KEYUP:
		pb::InputUp({InputTypes::Keyboard, (unsigned int)event->key.keysym.sym});
		break;
	case SDL_KEYDOWN:
		if (!event->key.repeat)
			pb::InputDown({InputTypes::Keyboard, (unsigned int)event->key.keysym.sym});
		switch (event->key.keysym.sym)
		{
		case SDLK_ESCAPE:
			if (Options.FullScreen)
				options::toggle(Menu1::Full_Screen);
			SDL_MinimizeWindow(MainWindow);
			break;
		case SDLK_F2:
			new_game();
			break;
		case SDLK_F3:
			pause();
			break;
		case SDLK_F4:
			options::toggle(Menu1::Full_Screen);
			break;
		case SDLK_F5:
			options::toggle(Menu1::Sounds);
			break;
		case SDLK_F6:
			options::toggle(Menu1::Music);
			break;
		case SDLK_F8:
			pause(false);
			options::ShowControlDialog();
			break;
		case SDLK_F9:
			options::toggle(Menu1::Show_Menu);
			break;
		default:
			break;
		}

		if (!pb::cheat_mode)
			break;

		switch (event->key.keysym.sym)
		{
		case SDLK_g:
			DispGRhistory ^= true;
			break;
		case SDLK_o:
			{
				auto plt = new ColorRgba[4 * 256];
				auto pltPtr = &plt[10]; // first 10 entries are system colors hardcoded in display_palette()
				for (int i1 = 0, i2 = 0; i1 < 256 - 10; ++i1, i2 += 8)
				{
					unsigned char blue = i2, redGreen = i2;
					if (i2 > 255)
					{
						blue = 255;
						redGreen = i1;
					}

					*pltPtr++ = ColorRgba{blue, redGreen, redGreen, 0};
				}
				gdrv::display_palette(plt);
				delete[] plt;
			}
			break;
		case SDLK_y:
			SDL_SetWindowTitle(MainWindow, "Pinball");
			DispFrameRate = DispFrameRate == 0;
			break;
		case SDLK_F1:
			pb::frame(10);
			break;
		case SDLK_F10:
			single_step ^= true;
			if (!single_step)
				no_time_loss = true;
			break;
		default:
			break;
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		{
			bool noInput = false;
			switch (event->button.button)
			{
			case SDL_BUTTON_LEFT:
				if (pb::cheat_mode)
				{
					mouse_down = 1;
					last_mouse_x = event->button.x;
					last_mouse_y = event->button.y;
					SDL_SetWindowGrab(MainWindow, SDL_TRUE);
					noInput = true;
				}
				break;
			default:
				break;
			}

			if (!noInput)
				pb::InputDown({InputTypes::Mouse, event->button.button});
		}
		break;
	case SDL_MOUSEBUTTONUP:
		{
			bool noInput = false;
			switch (event->button.button)
			{
			case SDL_BUTTON_LEFT:
				if (mouse_down)
				{
					mouse_down = 0;
					SDL_SetWindowGrab(MainWindow, SDL_FALSE);
					noInput = true;
				}
				break;
			default:
				break;
			}

			if (!noInput)
				pb::InputUp({InputTypes::Mouse, event->button.button});
		}
		break;
	case SDL_WINDOWEVENT:
		switch (event->window.event)
		{
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_TAKE_FOCUS:
		case SDL_WINDOWEVENT_SHOWN:
			activated = true;
			Sound::Activate();
			if (Options.Music && !single_step)
				midi::music_play();
			no_time_loss = true;
			has_focus = true;
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			activated = false;
			fullscrn::activate(0);
			Options.FullScreen = false;
			Sound::Deactivate();
			midi::music_stop();
			has_focus = false;
			pb::loose_focus();
			break;
		case SDL_WINDOWEVENT_SIZE_CHANGED:
		case SDL_WINDOWEVENT_RESIZED:
			fullscrn::window_size_changed();
			break;
		default: ;
		}
		break;
	default: ;
	}

	return 1;
}

int winmain::ProcessWindowMessages()
{
	static auto idleWait = 0;
	SDL_Event event;
	if (has_focus)
	{
		idleWait = static_cast<int>(TargetFrameTime.count());
		while (SDL_PollEvent(&event))
		{
			if (!event_handler(&event))
				return 0;
		}

		return 1;
	}

	// Progressively wait longer when transitioning to idle
	idleWait = std::min(idleWait + static_cast<int>(TargetFrameTime.count()), 500);
	if (SDL_WaitEventTimeout(&event, idleWait))
	{
		idleWait = static_cast<int>(TargetFrameTime.count());
		return event_handler(&event);
	}
	return 1;
}

void winmain::memalloc_failure()
{
	midi::music_stop();
	Sound::Close();
	const char* caption = pb::get_rc_string(Msg::STRING270);
	const char* text = pb::get_rc_string(Msg::STRING279);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption, text, MainWindow);
	std::exit(1);
}

void winmain::a_dialog()
{
	if (ShowAboutDialog == true)
	{
		ShowAboutDialog = false;
		ImGui::OpenPopup(pb::get_rc_string(Msg::STRING204));
	}

	bool unused_open = true;
	if (ImGui::BeginPopupModal(pb::get_rc_string(Msg::STRING204), &unused_open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted(pb::get_rc_string(Msg::STRING139));
		ImGui::TextUnformatted("Original game by Cinematronics, Microsoft");
		ImGui::Separator();

		ImGui::TextUnformatted("Decompiled -> Ported to SDL");
		ImGui::TextUnformatted("Version 2.0.1");
		if (ImGui::SmallButton("Project home: https://github.com/k4zmu2a/SpaceCadetPinball"))
		{
			SysAppBrowserArgs args = {
				{
					nullptr,
					0
				},
				"https://github.com/k4zmu2a/SpaceCadetPinball",
				strlen("https://github.com/k4zmu2a/SpaceCadetPinball")
			};
			SYSSwitchToBrowserForViewer(&args);
		}
		ImGui::Separator();

		if (ImGui::Button("Ok"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void winmain::end_pause()
{
	if (single_step)
	{
		pb::pause_continue();
		no_time_loss = true;
	}
}

void winmain::new_game()
{
	end_pause();
	pb::replay_level(false);
}

void winmain::pause(bool toggle)
{
	if (toggle || !single_step)
	{
		pb::pause_continue();
		no_time_loss = true;
	}
}

void winmain::Restart()
{
	restart = true;
	SDL_Event event{SDL_QUIT};
	SDL_PushEvent(&event);
}

void winmain::UpdateFrameRate()
{
	// UPS >= FPS
	auto fps = Options.FramesPerSecond, ups = Options.UpdatesPerSecond;
	UpdateToFrameRatio = static_cast<double>(ups) / fps;
	TargetFrameTime = DurationMs(1000.0 / ups);
}

void winmain::RenderFrameTimeDialog()
{
	if (!gfr_display)
		return;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{300, 70});
	if (ImGui::Begin("Frame Times", &DispGRhistory, ImGuiWindowFlags_NoScrollbar))
	{
		auto target = static_cast<float>(TargetFrameTime.count());
		auto scale = 1 / (gfr_display->Height / 2 / target);

		auto spin = Options.HybridSleep ? static_cast<float>(SpinThreshold.count()) : 0;
		ImGui::Text("Target frame time:%03.04fms, 1px:%03.04fms, SpinThreshold:%03.04fms",
		            target, scale, spin);
		gfr_display->BlitToTexture();
		auto region = ImGui::GetContentRegionAvail();
		ImGui::Image(gfr_display->Texture, region);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void winmain::HybridSleep(DurationMs sleepTarget)
{
	static constexpr double StdDevFactor = 0.5;

	// This nice concept is from https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
	// Sacrifices some CPU time for smaller frame time jitter
	while (sleepTarget > SpinThreshold)
	{
		auto start = Clock::now();
		std::this_thread::sleep_for(DurationMs(1));
		auto end = Clock::now();

		auto actualDuration = DurationMs(end - start);
		sleepTarget -= actualDuration;

		// Update expected sleep duration using Welford's online algorithm
		// With bad timer, this will run away to 100% spin
		SleepState.Advance(actualDuration.count());
		SpinThreshold = DurationMs(SleepState.mean + SleepState.GetStdDev() * StdDevFactor);
	}

	// spin lock
	for (auto start = Clock::now(); DurationMs(Clock::now() - start) < sleepTarget;);
}
