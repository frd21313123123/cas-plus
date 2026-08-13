#include "pch.h"
#include "Menu.hpp"

#include <algorithm>
#include <random>

namespace
{
	std::wstring makeRandomWindowTitle()
	{
		static constexpr wchar_t alphabet[] =
			L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		static std::random_device randomDevice;
		static std::mt19937 generator(randomDevice());
		static std::uniform_int_distribution<std::size_t> distribution(0, std::size(alphabet) - 2);

		std::wstring title;
		title.reserve(16);
		for (std::size_t i = 0; i < 16; ++i)
			title.push_back(alphabet[distribution(generator)]);

		return title;
	}
}

#include "dependency/imgui/imgui.h"
#include "dependency/imgui/imgui_internal.h"
#include "dependency/imgui/backend/imgui_impl_dx9.h"
#include "dependency/imgui/backend/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	void applyRoundedWindowRegion(HWND hWnd)
	{
		RECT clientRect{};
		if (!GetClientRect(hWnd, &clientRect))
			return;

		const int width = clientRect.right - clientRect.left;
		const int height = clientRect.bottom - clientRect.top;
		if (width <= 0 || height <= 0)
			return;

		const int radius = std::min(30, std::min(width, height) / 8);
		HRGN roundedRegion = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
		if (roundedRegion != nullptr && !SetWindowRgn(hWnd, roundedRegion, TRUE))
			DeleteObject(roundedRegion);
	}

	ImVec4 canvasColor(bool darkTheme)
	{
		return darkTheme
			? ImVec4(0.040f, 0.045f, 0.065f, 1.0f)
			: ImVec4(0.955f, 0.970f, 0.990f, 1.0f);
	}

	ImVec4 outerCanvasColor(bool darkTheme)
	{
		return darkTheme
			? ImVec4(0.065f, 0.075f, 0.110f, 1.0f)
			: ImVec4(0.875f, 0.910f, 0.960f, 1.0f);
	}

	void renderKeybindBadge(const char* label, const char* keyText)
	{
		ImGui::TextUnformatted(label);
		ImGui::SameLine(ImGui::GetWindowWidth() - 75.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.15f, 0.28f, 1.0f));
		ImGui::Button(keyText, ImVec2(60.0f, 20.0f));
		ImGui::PopStyleColor();
	}
}

bool Menu::initialize()
{
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, Menu::WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("WC"), NULL };
	::RegisterClassEx(&wc);
	const auto nativeWindowTitle = makeRandomWindowTitle();
	this->hwnd = ::CreateWindow(wc.lpszClassName, nativeWindowTitle.c_str(),
		WS_POPUP,
		100, 100, 780, 620, NULL, NULL, wc.hInstance, NULL);
	if (this->hwnd == nullptr)
	{
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}
	applyRoundedWindowRegion(this->hwnd);

	if (!createD3D9Device(hwnd))
	{
		cleanupD3D9Device();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.WantSaveIniSettings = false;

	setupMenuStyle(true, 1);

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(this->d3dDevice);

	this->isMenuOn = true;
	std::thread(&Menu::detectGame, this).detach();
	std::thread(&Menu::detectSteam, this).detach();
	std::thread(&Menu::updateFiles, this).detach();

	return true;
}

void Menu::loop()
{
	while (this->isMenuOn)
	{
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				this->isMenuOn = false;
		}
		if (!this->isMenuOn)
			break;

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		constexpr float outerMargin = 6.0f;
		ImGui::SetNextWindowPos(ImVec2(outerMargin, outerMargin), ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			ImVec2(std::max(0.0f, displaySize.x - outerMargin * 2.0f),
				std::max(0.0f, displaySize.y - outerMargin * 2.0f)),
			ImGuiCond_Always);
		ImGui::Begin("cas v2.3", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoScrollbar);

		// Top Header
		ImGui::BeginChild("Header", ImVec2(0, 52), true);
		ImGui::TextColored(ImVec4(0.65f, 0.45f, 1.00f, 1.0f), "CAS v2.3");
		ImGui::SameLine();
		ImGui::TextDisabled(" | Nixware UI Replica");
		ImGui::SameLine(ImGui::GetWindowWidth() - 36.0f);
		if (ImGui::Button("X", ImVec2(28.0f, 26.0f)))
			::PostMessage(hwnd, WM_CLOSE, 0, 0);
		ImGui::EndChild();
		ImGui::Spacing();

		// Main Nav Tabs (CLICKABLE)
		static const char* mainTabs[] = { "Ragebot", "Anti-Aim", "Visuals", "Misc", "Configs" };
		const float tabWidth = (ImGui::GetWindowWidth() - 10.0f - (4.0f * 6.0f)) / 5.0f;
		for (int i = 0; i < 5; ++i)
		{
			if (i > 0) ImGui::SameLine(0.0f, 6.0f);
			const bool isSelected = (this->activeTab == i);
			if (isSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.48f, 0.30f, 0.88f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.36f, 0.96f, 1.0f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.13f, 0.19f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.20f, 0.28f, 1.0f));
			}
			if (ImGui::Button(mainTabs[i], ImVec2(tabWidth, 34.0f)))
			{
				this->activeTab = i;
				this->activeSubtab = 0;
			}
			ImGui::PopStyleColor(2);
		}

		// Subtabs (CLICKABLE)
		ImGui::Spacing();
		std::vector<const char*> subtabs;
		if (this->activeTab == 0) subtabs = { "General" };
		else if (this->activeTab == 1) subtabs = { "General", "Builder", "Adaptive" };
		else if (this->activeTab == 2) subtabs = { "General", "World", "Local player", "Extra", "Chams" };
		else if (this->activeTab == 3) subtabs = { "General", "Shared features" };
		else if (this->activeTab == 4) subtabs = { "General" };

		for (size_t i = 0; i < subtabs.size(); ++i)
		{
			if (i > 0) ImGui::SameLine(0.0f, 6.0f);
			const bool isSubSelected = (this->activeSubtab == static_cast<int>(i));
			if (isSubSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.22f, 0.65f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.28f, 0.75f, 1.0f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.10f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.17f, 0.24f, 1.0f));
			}
			if (ImGui::Button(subtabs[i], ImVec2(120.0f, 26.0f)))
				this->activeSubtab = static_cast<int>(i);
			ImGui::PopStyleColor(2);
		}
		ImGui::Spacing();

		// Content Panel (READ-ONLY / DISABLED FOR ALL INNER CONTROLS)
		ImGui::BeginChild("ContentArea", ImVec2(0, 0), true);
		ImGui::BeginDisabled(true);

		// Render active tab & subtab content
		if (this->activeTab == 0) // Ragebot
		{
			ImGui::Columns(2, nullptr, false);
			ImGui::TextDisabled("Main - Movement assists");
			ImGui::Separator();
			renderKeybindBadge("Edge jump", "[ALT]");
			static bool duckpeek = false; ImGui::Checkbox("Duck peek assist", &duckpeek);
			static int duckdmg = 93; ImGui::SliderInt("Duck peek minimal damage", &duckdmg, 1, 130);
			renderKeybindBadge("Duck peek assist bind", "[X]");
			static bool aipeek = false; ImGui::Checkbox("Auto peek assistant", &aipeek);
			renderKeybindBadge("Auto peek assistant bind", "[M4]");
			static int aipdmg = 93; ImGui::SliderInt("Auto peek minimal damage", &aipdmg, 1, 130);

			ImGui::NextColumn();
			ImGui::TextDisabled("Extra - Damage override");
			ImGui::Separator();
			renderKeybindBadge("Override minimal damage bind", "[M5]");
			static int ovdmg = 45; ImGui::SliderInt("Override minimal damage", &ovdmg, 1, 130);

			ImGui::Spacing(); ImGui::Spacing();
			ImGui::TextDisabled("Air aim - Airborne target assist");
			ImGui::Separator();
			static bool airaim = false; ImGui::Checkbox("Air Aim Assist", &airaim);
			static bool airatk = true; ImGui::Checkbox("Only while attacking", &airatk);
			static int airhitbox = 1; ImGui::Combo("Air hitbox priority", &airhitbox, "Head only\0Head + chest\0Smart\0");
			static int airfov = 12; ImGui::SliderInt("Air maximum FOV", &airfov, 1, 45, "%d°");
			static int airdmg = 45; ImGui::SliderInt("Air minimal damage", &airdmg, 1, 130);
			static int airstr = 100; ImGui::SliderInt("Aim strength", &airstr, 20, 100, "%d%%");
			static bool airfire = false; ImGui::Checkbox("Air auto fire", &airfire);
			ImGui::Columns(1);
		}
		else if (this->activeTab == 1) // Anti-Aim
		{
			if (this->activeSubtab == 0) // General
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("Main - Manual direction");
				ImGui::Separator();
				static bool manena = true; ImGui::Checkbox("Manual direction", &manena);
				static int baseyaw = 180; ImGui::SliderInt("Fallback base yaw", &baseyaw, -180, 180, "%d°");
				static int manoff = 97; ImGui::SliderInt("Manual yaw offset", &manoff, 0, 180, "%d°");
				renderKeybindBadge("Manual left", "[Z]");
				renderKeybindBadge("Manual right", "[C]");
				static bool staticman = true; ImGui::Checkbox("Static manuals", &staticman);
				static bool manind = true; ImGui::Checkbox("Manuals indication", &manind);

				ImGui::NextColumn();
				ImGui::TextDisabled("Other - Movement");
				ImGui::Separator();
				static bool duckjump = false; ImGui::Checkbox("Duck Jump [WIP]", &duckjump);
				renderKeybindBadge("Duck Jump bind", "[SPACE]");

				ImGui::Spacing(); ImGui::Spacing();
				ImGui::TextDisabled("Fake Duck");
				ImGui::Separator();
				static bool fakeduck = false; ImGui::Checkbox("Fake Duck", &fakeduck);
				renderKeybindBadge("Fake Duck bind", "[V]");
				static bool fdground = true; ImGui::Checkbox("Ground only", &fdground);
				static int fdhigh = 82; ImGui::SliderInt("Crouch switch threshold", &fdhigh, 55, 100, "%d%%");
				static int fdlow = 20; ImGui::SliderInt("Stand switch threshold", &fdlow, 0, 45, "%d%%");
				static int fddelay = 30; ImGui::SliderInt("Minimum phase time", &fddelay, 0, 180, "%d ms");
				ImGui::Columns(1);
			}
			else if (this->activeSubtab == 1) // Builder
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("Main - Anti-Aim Builder");
				ImGui::Separator();
				static bool aabuilder = true; ImGui::Checkbox("Anti-Aim Builder", &aabuilder);
				static int aacond = 0; ImGui::Combo("Condition", &aacond, "Stand\0Run\0In-Air\0Air-Duck\0Duck\0Duck-Walk\0");

				ImGui::NextColumn();
				ImGui::TextDisabled("Profile Settings");
				ImGui::Separator();
				static bool stena = true; ImGui::Checkbox("Enabled", &stena);
				static int stbase = 180; ImGui::SliderInt("Base Yaw", &stbase, -180, 180, "%d°");
				static int stmod = 1; ImGui::Combo("Yaw Modifier", &stmod, "None\0Center\0Offset\0Random\0 3-Way\0 5-Way\0Spin\0");
				static int moff = 58; ImGui::SliderInt("Modifier Offset", &moff, 0, 180, "%d°");
				static bool rnd = false; ImGui::Checkbox("Controlled base randomization", &rnd);
				static int rndr = 24; ImGui::SliderInt("Random Range", &rndr, 0, 180, "%d°");
				static int rndi = 260; ImGui::SliderInt("Random Interval", &rndi, 50, 1500, "%d ms");
				ImGui::Columns(1);
			}
			else // Adaptive
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("Global");
				ImGui::Separator();
				static int pitch = 1; ImGui::Combo("Pitch [Global]", &pitch, "None\0Down\0Fake\0");
				static bool aaind = true; ImGui::Checkbox("AA state indicator", &aaind);

				ImGui::Spacing(); ImGui::Spacing();
				ImGui::TextDisabled("Freestanding / Auto Direction");
				ImGui::Separator();
				static bool freeena = false; ImGui::Checkbox("Freestanding / Auto Direction", &freeena);
				renderKeybindBadge("Freestanding on key", "[NONE]");
				static int freedelta = 90; ImGui::SliderInt("Freestanding yaw", &freedelta, 45, 180, "%d°");
				static int freeint = 120; ImGui::SliderInt("Scan interval", &freeint, 50, 500, "%d ms");

				ImGui::NextColumn();
				ImGui::TextDisabled("Air Flick");
				ImGui::Separator();
				static bool airf = false; ImGui::Checkbox("Air Flick", &airf);
				static int airfint = 420; ImGui::SliderInt("Air Flick interval", &airfint, 120, 1500, "%d ms");
				static int airfdelta = 95; ImGui::SliderInt("Air Flick yaw", &airfdelta, 45, 180, "%d°");

				ImGui::Spacing(); ImGui::Spacing();
				ImGui::TextDisabled("Landing AA");
				ImGui::Separator();
				static bool landaa = false; ImGui::Checkbox("Landing AA", &landaa);
				static int landdur = 120; ImGui::SliderInt("Landing duration", &landdur, 20, 500, "%d ms");
				static int landyaw = 180; ImGui::SliderInt("Landing yaw", &landyaw, -180, 180, "%d°");
				static bool landzero = true; ImGui::Checkbox("Zero pitch on land", &landzero);
				ImGui::Columns(1);
			}
		}
		else if (this->activeTab == 2) // Visuals
		{
			if (this->activeSubtab == 0) // General
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("Camera");
				ImGui::Separator();
				static int tpdist = 350; ImGui::SliderInt("Thirdperson distance", &tpdist, 0, 10000);
				static bool tpzoom = false; ImGui::Checkbox("Thirdperson wheel zoom", &tpzoom);
				static int tpstep = 100; ImGui::SliderInt("Wheel zoom step", &tpstep, 10, 1000);
				static int wfov = 90; ImGui::SliderInt("Override FOV", &wfov, 65, 140, "%d°");

				ImGui::Spacing(); ImGui::Spacing();
				ImGui::TextDisabled("Scope");
				ImGui::Separator();
				static bool noscope = false; ImGui::Checkbox("Disable scope zoom", &noscope);
				static bool custscope = false; ImGui::Checkbox("Override scope overlay", &custscope);
				static float scolor[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; ImGui::ColorEdit4("First color", scolor);
				static int ssize = 50; ImGui::SliderInt("Line size", &ssize, 1, 250);
				static int sgap = 32; ImGui::SliderInt("Line gap", &sgap, 0, 250);
				static int sfade = 12; ImGui::SliderInt("Line gradient size", &sfade, 0, 250);

				ImGui::NextColumn();
				ImGui::TextDisabled("Bullet visualization");
				ImGui::Separator();
				static bool btracers = false; ImGui::Checkbox("Local bullet tracers", &btracers);
				static int tdur = 4; ImGui::SliderInt("Tracers duration", &tdur, 1, 10, "%d s");
				static float tcol[4] = { 0.25f, 0.75f, 1.0f, 1.0f }; ImGui::ColorEdit4("Tracers color", tcol);
				static bool bimpacts = false; ImGui::Checkbox("Bullet impacts", &bimpacts);
				static int idur = 4; ImGui::SliderInt("Impacts duration", &idur, 1, 10, "%d s");
				static float icol[4] = { 1.0f, 0.25f, 0.25f, 1.0f }; ImGui::ColorEdit4("Impacts color", icol);
				ImGui::Columns(1);
			}
			else if (this->activeSubtab == 1) // World
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("World");
				ImGui::Separator();
				static bool custsky = false; ImGui::Checkbox("Custom skybox", &custsky);
				static int skycombo = 0; ImGui::Combo("Skybox", &skycombo, "Cloudy\0Anubis\0Dust 2\0Mirage\0Nuke\0Overpass\0Train\0Vertigo\0Aztec\0Italy\0");
				ImGui::Button("Apply selected skybox", ImVec2(-1, 30));

				ImGui::NextColumn();
				ImGui::TextDisabled("Wall penetration");
				ImGui::Separator();
				static bool wallbang = false; ImGui::Checkbox("Highlight penetrable surfaces", &wallbang);
				static float wbcol[4] = { 0.3f, 1.0f, 0.55f, 1.0f }; ImGui::ColorEdit4("Surface highlight color", wbcol);
				static bool wbcross = false; ImGui::Checkbox("Wallbang crosshair indicator", &wbcross);
				static float wbok[4] = { 0.3f, 1.0f, 0.55f, 1.0f }; ImGui::ColorEdit4("Penetrable color", wbok);
				static float wbbad[4] = { 1.0f, 0.25f, 0.25f, 1.0f }; ImGui::ColorEdit4("Blocked color", wbbad);
				static int wbdepth = 48; ImGui::SliderInt("Probe depth", &wbdepth, 8, 128);
				static bool wbdmg = true; ImGui::Checkbox("Show penetration damage", &wbdmg);
				ImGui::Columns(1);
			}
			else if (this->activeSubtab == 2) // Local player
			{
				ImGui::TextDisabled("Local model");
				ImGui::Separator();
				static bool localoutline = false; ImGui::Checkbox("Local model fallback outline", &localoutline);
			}
			else if (this->activeSubtab == 3) // Extra
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("Grenades");
				ImGui::Separator();
				static bool gtrail = false; ImGui::Checkbox("Grenade trail", &gtrail);
				static int gtdur = 5; ImGui::SliderInt("Grenade trail duration", &gtdur, 1, 10, "%d s");
				static bool gradius = false; ImGui::Checkbox("Grenade Radius", &gradius);
				static float mcol[4] = { 1.0f, 0.25f, 0.25f, 1.0f }; ImGui::ColorEdit4("Molotov Radius Color", mcol);
				static float scol[4] = { 0.65f, 0.8f, 1.0f, 1.0f }; ImGui::ColorEdit4("Smoke Radius Color", scol);

				ImGui::NextColumn();
				ImGui::TextDisabled("Sounds");
				ImGui::Separator();
				static bool hitsound = false; ImGui::Checkbox("Custom Hit Sound", &hitsound);
				static char soundname[128] = "sounds\\music\\revenge.vsnd"; ImGui::InputText("Sound name", soundname, sizeof(soundname));
				static int hitvol = 40; ImGui::SliderInt("Hit Sound volume", &hitvol, 1, 100, "%d%%");
				ImGui::Columns(1);
			}
			else // Chams
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("Chams");
				ImGui::Separator();
				static bool chamsena = false; ImGui::Checkbox("Enable", &chamsena);
				static bool chamsloc = true; ImGui::Checkbox("Local Model", &chamsloc);
				static bool chamsatt = false; ImGui::Checkbox("Attachments", &chamsatt);

				ImGui::NextColumn();
				ImGui::TextDisabled("Chams Settings");
				ImGui::Separator();
				static int loctype = 0; ImGui::Combo("Local Model Chams Type", &loctype, "Glass\0Glow\0");
				static float loccol[4] = { 0.65f, 0.43f, 1.0f, 1.0f }; ImGui::ColorEdit4("Local Model Chams Color", loccol);
				static int atttype = 1; ImGui::Combo("Attachments Chams Type", &atttype, "Glass\0Glow\0");
				static float attcol[4] = { 0.25f, 0.75f, 1.0f, 1.0f }; ImGui::ColorEdit4("Attachments Chams Color", attcol);
				ImGui::Columns(1);
			}
		}
		else if (this->activeTab == 3) // Misc
		{
			if (this->activeSubtab == 0) // General
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::TextDisabled("Windows");
				ImGui::Separator();
				static bool wmark = true; ImGui::Checkbox("Watermark", &wmark);
				static bool wbinds = true; ImGui::Checkbox("Keybinds", &wbinds);
				static bool wbomb = true; ImGui::Checkbox("Bomb Info", &wbomb);
				static bool wtips = true; ImGui::Checkbox("Setting tooltips", &wtips);

				ImGui::Spacing(); ImGui::Spacing();
				ImGui::TextDisabled("Match statistics");
				ImGui::Separator();
				static bool skd = false; ImGui::Checkbox("Show K/D", &skd);
				static bool skills = false; ImGui::Checkbox("Show kills", &skills);
				static bool sdeaths = false; ImGui::Checkbox("Show deaths", &sdeaths);

				ImGui::NextColumn();
				ImGui::TextDisabled("Player model");
				ImGui::Separator();
				static bool agentena = false; ImGui::Checkbox("Agent changer enabled", &agentena);
				static int agentcombo = 0; ImGui::Combo("Agent changer", &agentcombo, "Custom\0Special Agent Ava | FBI\0Operator | FBI SWAT\0Markus Delrow | FBI HRT\0");
				static char cmodel[128] = "agents/models/ctm_fbi/ctm_fbi_variantb.vmdl"; ImGui::InputText("Custom model", cmodel, sizeof(cmodel));
				static bool agentcross = false; ImGui::Checkbox("Allow cross-team agent", &agentcross);
				ImGui::Button("Apply selected agent", ImVec2(-1, 30));

				ImGui::Spacing(); ImGui::Spacing();
				ImGui::TextDisabled("Buy bot");
				ImGui::Separator();
				static bool buyena = false; ImGui::Checkbox("Buy bot", &buyena);
				static int buyprim = 0; ImGui::Combo("Primary weapon", &buyprim, "None\0SCAR20/G3SG1\0SSG08\0AWP\0AK-47/M4A1(-S)\0");
				static int buysec = 0; ImGui::Combo("Secondary weapon", &buysec, "None\0FN57/TEC9\0Dual Elites\0Deagle\0Revolver\0");
				static bool eqfire = false; ImGui::Checkbox("Fire grenade/Molotov", &eqfire);
				static bool eqsmoke = false; ImGui::Checkbox("Smoke grenade", &eqsmoke);
				static bool eqhe = false; ImGui::Checkbox("Explosive grenade", &eqhe);
				static bool eqvest = false; ImGui::Checkbox("Kevlar", &eqvest);
				static bool eqhelm = false; ImGui::Checkbox("Helmet", &eqhelm);
				ImGui::Columns(1);
			}
			else // Shared features
			{
				ImGui::TextDisabled("Shared features");
				ImGui::Separator();
				static bool spamena = false; ImGui::Checkbox("Clantag\\Name spammer", &spamena);
				static int spamtype = 0; ImGui::Combo("Clantag type", &spamtype, "Default\0Custom\0");
				static char spamtext[64] = "cas"; ImGui::InputText("Custom text", spamtext, sizeof(spamtext));
			}
		}
		else // Configs
		{
			ImGui::Columns(2, nullptr, false);
			ImGui::TextDisabled("Actions");
			ImGui::Separator();
			static int cfgslot = 0; ImGui::Combo("List", &cfgslot, "Slot 1\0Slot 2\0Slot 3\0Slot 4\0Slot 5\0");
			ImGui::Spacing();
			ImGui::Button("Load config", ImVec2(-1, 32));
			ImGui::Button("Save config", ImVec2(-1, 32));
			ImGui::Button("Reset config", ImVec2(-1, 32));

			ImGui::NextColumn();
			ImGui::TextDisabled("Config Status");
			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.55f, 1.0f), "Status: Slot 1 ready");
			ImGui::TextDisabled("Last updated: Just now");
			ImGui::Columns(1);
		}

		ImGui::EndDisabled();
		ImGui::EndChild();

		ImGui::End();

		ImGui::EndFrame();

		this->d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		const ImVec4 clearColor = outerCanvasColor(this->isDarkTheme);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clearColor.x * 255.0f), (int)(clearColor.y * 255.0f), (int)(clearColor.z * 255.0f), 255);
		this->d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if (this->d3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			this->d3dDevice->EndScene();
		}
		HRESULT result = this->d3dDevice->Present(NULL, NULL, NULL, NULL);
	}
}

bool Menu::createD3D9Device(HWND hWnd)
{
	if ((this->pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) return false;

	ZeroMemory(&this->d3dpp, sizeof(this->d3dpp));
	this->d3dpp.Windowed = TRUE;
	this->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	this->d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; 
	this->d3dpp.EnableAutoDepthStencil = TRUE;
	this->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	this->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;       
	this->d3dpp.hDeviceWindow = hWnd;
	auto result = this->pD3D->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hwnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&this->d3dpp, &this->d3dDevice);
	if (result != S_OK) return false;

	return true;
}

void Menu::cleanupD3D9Device()
{
	if (this->d3dDevice != nullptr)
	{
		this->d3dDevice->Release();
		this->d3dDevice = nullptr;
	}

	if (this->pD3D != nullptr)
	{
		this->pD3D->Release();
		this->pD3D = nullptr;
	}
}

LRESULT __stdcall Menu::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_NCHITTEST:
	{
		POINT cursor{
			static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
			static_cast<LONG>(static_cast<short>(HIWORD(lParam))) };
		::ScreenToClient(hWnd, &cursor);
		if (cursor.y >= 0 && cursor.y < 52 && cursor.x < 700)
			return HTCAPTION;
		break;
	}
	case WM_SIZE:
		applyRoundedWindowRegion(hWnd);
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void Menu::renderStatusPanel() {}
void Menu::renderTargetPanel() {}
std::vector<std::string> Menu::snapshotDllPaths() { return {}; }
void Menu::renderInjectionPanel(const std::vector<std::string>& paths) {}

void Menu::setupMenuStyle(bool isDarkTheme, float alpha)
{
	ImGuiStyle& style = ImGui::GetStyle();

	style = ImGuiStyle();
	style.Alpha = alpha;
	style.WindowPadding = ImVec2(12.0f, 12.0f);
	style.FramePadding = ImVec2(10.0f, 7.0f);
	style.ItemSpacing = ImVec2(10.0f, 8.0f);
	style.ItemInnerSpacing = ImVec2(7.0f, 6.0f);
	style.WindowRounding = 16.0f;
	style.ChildRounding = 10.0f;
	style.FrameRounding = 8.0f;
	style.PopupRounding = 8.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 6.0f;
	style.TabRounding = 8.0f;
	style.WindowBorderSize = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;

	const ImVec4 accent = ImVec4(0.55f, 0.35f, 0.95f, 1.0f);
	const ImVec4 accentHover = ImVec4(0.65f, 0.45f, 1.00f, 1.0f);

	style.Colors[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 0.98f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.54f, 0.65f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.07f, 0.10f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.15f, 1.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.15f, 1.0f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.18f, 0.20f, 0.28f, 0.8f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.20f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.19f, 0.28f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.24f, 0.35f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = accentHover;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_SliderGrabActive] = accentHover;
	style.Colors[ImGuiCol_Button] = ImVec4(0.14f, 0.16f, 0.24f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.25f, 0.36f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = accent;
	style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];
}

void Menu::detectSteam() {}
void Menu::detectGame() {}
void Menu::updateFiles() {}
