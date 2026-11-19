// The HUD, drawn through F4SE Menu Framework when it's installed, in Playable Pool's and Wasteland Wagers' green
// terminal style: the game, your score and best, the clock, the power meter and aim guide, the race, the points as
// they come and the end of the game's banner (the tickets it paid). Text uses the two pre-rendered fonts in
// Data\F4SE\Plugins\AmusementGames\hud (HudFont.h); the panels are drawn shapes. Laid out on a 1280x720 design
// screen, scaled to the real one.

#include "Hud.h"

#include "Arcade.h"
#include "F4SEMenuFramework.h"
#include "HudFont.h"

namespace ag::Hud
{
	namespace
	{
		using namespace ImGuiMCP;
		namespace DL = ImGuiMCP::ImDrawListManager;

		constexpr ImU32 Rgba(int r, int g, int b, float a = 1.0f)
		{
			return static_cast<ImU32>(r) | (static_cast<ImU32>(g) << 8) | (static_cast<ImU32>(b) << 16) |
			       (static_cast<ImU32>(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f) << 24);
		}
		ImU32 Fade(ImU32 a_col, float a_alpha)
		{
			const float a = static_cast<float>(a_col >> 24) / 255.0f * std::clamp(a_alpha, 0.0f, 1.0f);
			return (a_col & 0x00FFFFFFu) | (static_cast<ImU32>(a * 255.0f + 0.5f) << 24);
		}

		constexpr ImU32 kBright = Rgba(166, 255, 166);
		constexpr ImU32 kText = Rgba(134, 255, 134);
		constexpr ImU32 kDim = Rgba(95, 174, 102);
		constexpr ImU32 kOff = Rgba(79, 143, 86);
		constexpr ImU32 kGlow = Rgba(80, 255, 110, 0.55f);
		constexpr ImU32 kRedText = Rgba(255, 122, 110);
		constexpr ImU32 kRedGlow = Rgba(255, 60, 40, 0.6f);
		constexpr ImU32 kPanel = Rgba(6, 20, 9, 0.78f);
		constexpr ImU32 kEdge = Rgba(80, 200, 100, 0.55f);
		constexpr ImU32 kRedChip = Rgba(178, 22, 30);
		constexpr ImU32 kBlackChip = Rgba(28, 26, 26);
		constexpr ImU32 kGreenChip = Rgba(18, 112, 40);

		// ------------------------------------------------------------ fonts

		struct Tex
		{
			ImTextureID id{ nullptr };
			bool        tried{ false };
		};
		std::unordered_map<std::string, Tex> g_textures;
		bool                                 g_artMissing{ false };

		ImTextureID Texture(const char* a_file)
		{
			auto& t = g_textures[a_file];
			if (!t.tried) {
				t.tried = true;
				for (const char* dir : { "Data\\F4SE\\Plugins\\AmusementGames\\hud\\", "F4SE\\Plugins\\AmusementGames\\hud\\" }) {
					t.id = F4SEMenuFramework::LoadTexture(std::string(dir) + a_file);
					if (t.id) {
						break;
					}
				}
				if (!t.id) {
					g_artMissing = true;
					log::warn("HUD: could not load {}; the plain HUD is used", a_file);
				}
			}
			return t.id;
		}

		struct View
		{
			ImDrawList* dl;
			ImVec2      origin;
			float       s;
			float       w, h;

			ImVec2 P(float a_x, float a_y) const { return { origin.x + a_x * s, origin.y + a_y * s }; }
		};

		const HudFont::Glyph* FindGlyph(const HudFont::Font& a_f, std::uint32_t a_code)
		{
			for (int i = 0; i < a_f.count; ++i) {
				if (a_f.glyphs[i].code == a_code) {
					return &a_f.glyphs[i];
				}
			}
			return a_code != '?' ? FindGlyph(a_f, '?') : nullptr;
		}

		std::vector<std::uint32_t> Decode(std::string_view a_text)
		{
			std::vector<std::uint32_t> out;
			for (std::size_t i = 0; i < a_text.size();) {
				const auto    c = static_cast<unsigned char>(a_text[i]);
				std::uint32_t cp = c;
				int           extra = 0;
				if (c >= 0xF0) {
					cp = c & 0x07;
					extra = 3;
				} else if (c >= 0xE0) {
					cp = c & 0x0F;
					extra = 2;
				} else if (c >= 0xC0) {
					cp = c & 0x1F;
					extra = 1;
				}
				++i;
				for (int k = 0; k < extra && i < a_text.size(); ++k, ++i) {
					cp = (cp << 6) | (static_cast<unsigned char>(a_text[i]) & 0x3F);
				}
				out.push_back(cp);
			}
			return out;
		}

		float TextWidth(const HudFont::Font& a_f, std::string_view a_text, float a_size, float a_spacing)
		{
			const float k = a_size / a_f.size;
			float       w = 0.0f;
			const auto  codes = Decode(a_text);
			for (std::size_t i = 0; i < codes.size(); ++i) {
				if (const auto* g = FindGlyph(a_f, codes[i])) {
					w += g->advance * k + (i + 1 < codes.size() ? a_spacing : 0.0f);
				}
			}
			return w;
		}

		enum class Align
		{
			kLeft,
			kCentre,
			kRight
		};

		void Print(const View& a_v, const HudFont::Font& a_f, std::string_view a_text, float a_x, float a_y, float a_size, ImU32 a_col,
			ImU32 a_glow = 0, float a_spacing = 0.0f, Align a_align = Align::kLeft)
		{
			auto sharp = Texture(a_f.file);
			auto glow = (a_glow >> 24) ? Texture(a_f.glowFile) : nullptr;
			if (!sharp) {
				return;
			}
			const float k = a_size / a_f.size;
			const float width = TextWidth(a_f, a_text, a_size, a_spacing);
			float       x = a_align == Align::kLeft ? a_x : a_align == Align::kCentre ? a_x - width * 0.5f : a_x - width;
			for (const auto code : Decode(a_text)) {
				const auto* g = FindGlyph(a_f, code);
				if (!g) {
					continue;
				}
				if (code != ' ') {
					const ImVec2 p0 = a_v.P(x + g->xoff * k, a_y + g->yoff * k);
					const ImVec2 p1 = a_v.P(x + (g->xoff + g->w) * k, a_y + (g->yoff + g->h) * k);
					const ImVec2 uv0{ g->x / a_f.atlasW, g->y / a_f.atlasH };
					const ImVec2 uv1{ (g->x + g->w) / a_f.atlasW, (g->y + g->h) / a_f.atlasH };
					if (glow) {
						DL::AddImage(a_v.dl, glow, p0, p1, uv0, uv1, a_glow);
					}
					DL::AddImage(a_v.dl, sharp, p0, p1, uv0, uv1, a_col);
				}
				x += g->advance * k + a_spacing;
			}
		}

		// text centred on (a_cx, a_cy) by its ink, not its advance and line height: digits sit dead centre in a disc
		void PrintInkCentred(const View& a_v, const HudFont::Font& a_f, std::string_view a_text, float a_cx, float a_cy, float a_size, ImU32 a_col)
		{
			const float k = a_size / a_f.size;
			const auto* space = FindGlyph(a_f, ' ');
			const float pad = space ? -space->xoff : 0.0f;  // the atlas's padding round each glyph (for the glow)
			float       pen = 0.0f, l = 1.0e9f, r = -1.0e9f, t = 1.0e9f, b = -1.0e9f;
			for (const auto code : Decode(a_text)) {
				const auto* g = FindGlyph(a_f, code);
				if (!g) {
					continue;
				}
				if (code != ' ') {
					l = std::min(l, pen + (g->xoff + pad) * k);
					r = std::max(r, pen + (g->xoff + g->w - pad) * k);
					t = std::min(t, (g->yoff + pad) * k);
					b = std::max(b, (g->yoff + g->h - pad) * k);
				}
				pen += g->advance * k;
			}
			if (r < l) {
				return;
			}
			Print(a_v, a_f, a_text, a_cx - (l + r) * 0.5f, a_cy - (t + b) * 0.5f, a_size, a_col);
		}

		const HudFont::Font& Title() { return HudFont::k_title; }
		const HudFont::Font& Mono() { return HudFont::k_mono; }

		// ------------------------------------------------------------ shapes

		void Chamfer(const View& a_v, float a_x, float a_y, float a_w, float a_h, float a_c, ImU32 a_fill, ImU32 a_edge)
		{
			const std::array<ImVec2, 8> pts{ a_v.P(a_x + a_c, a_y), a_v.P(a_x + a_w - a_c, a_y), a_v.P(a_x + a_w, a_y + a_c),
				a_v.P(a_x + a_w, a_y + a_h - a_c), a_v.P(a_x + a_w - a_c, a_y + a_h), a_v.P(a_x + a_c, a_y + a_h), a_v.P(a_x, a_y + a_h - a_c),
				a_v.P(a_x, a_y + a_c) };
			DL::AddConvexPolyFilled(a_v.dl, pts.data(), static_cast<int>(pts.size()), a_fill);
			if (a_edge >> 24) {
				DL::AddPolyline(a_v.dl, pts.data(), static_cast<int>(pts.size()), a_edge, ImDrawFlags_Closed, std::max(1.0f, 1.2f * a_v.s));
			}
		}


		std::string Commas(int a_n)
		{
			std::string s = std::to_string(std::abs(a_n));
			for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
				s.insert(static_cast<std::size_t>(i), ",");
			}
			return a_n < 0 ? "-" + s : s;
		}

		// ------------------------------------------------------------ the pieces

		std::string Clock(double a_s)
		{
			const int s = static_cast<int>(std::ceil(std::max(0.0, a_s)));
			return std::format("{}:{:02}", s / 60, s % 60);
		}

		// top middle: the clock (red in the last ten seconds), or an untimed game's stand-in (the ball you're on, the
		// race's countdown)
		void Timer(const View& a_v, const Arcade::HudState& a_st)
		{
			std::string text;
			bool        hurry = false;
			if (a_st.info.timeLeft >= 0.0) {
				text = Clock(a_st.info.timeLeft);
				hurry = a_st.info.timeLeft <= 10.0;
			} else {
				text = a_st.info.clock;
			}
			if (text.empty()) {
				return;
			}
			const float w = std::max(150.0f, TextWidth(Title(), text, 40, 2.0f) + 40.0f), x = a_v.w * 0.5f - w * 0.5f, y = 24.0f;
			Chamfer(a_v, x, y, w, 64.0f, 10.0f, kPanel, kEdge);
			Print(a_v, Title(), text, x + w * 0.5f, y + 8, 40, hurry ? kRedText : kBright, hurry ? kRedGlow : kGlow, 2.0f, Align::kCentre);
		}

		// bottom middle: the throw's power while you hold the button, and (skee-ball) where across the lane it goes
		void Meter(const View& a_v, const Arcade::HudState& a_st)
		{
			if (a_st.info.power < 0.0) {
				return;
			}
			const float w = 360.0f, x = a_v.w * 0.5f - w * 0.5f, y = a_v.h - 120.0f;
			Chamfer(a_v, x - 18, y - 30, w + 36, 70.0f, 10.0f, kPanel, kEdge);
			if (a_st.info.power >= 0.0) {
				Print(a_v, Mono(), "POWER", x, y - 22, 12, kDim, 0, 2.0f);
				DL::AddRect(a_v.dl, a_v.P(x, y), a_v.P(x + w, y + 18), kEdge, 0.0f, 0, std::max(1.0f, 1.2f * a_v.s));
				const float p = static_cast<float>(std::clamp(a_st.info.power, 0.0, 1.0));
				const ImU32 col = p > 0.85f ? kRedText : kText;
				DL::AddRectFilled(a_v.dl, a_v.P(x + 2, y + 2), a_v.P(x + 2 + (w - 4) * p, y + 16), col, 0.0f, 0);
				for (int k = 1; k < 4; ++k) {
					const float tx = x + w * k / 4.0f;
					DL::AddLine(a_v.dl, a_v.P(tx, y + 12), a_v.P(tx, y + 18), kDim, std::max(1.0f, a_v.s));
				}
			}
		}

		// the points as they come (Atomic Rollers): big in the middle, rising and fading
		void Points(const View& a_v, const Arcade::HudState& a_st)
		{
			if (a_st.sinceLastPoints > 1.2 || a_st.lastPoints <= 0) {
				return;
			}
			const float t = static_cast<float>(a_st.sinceLastPoints / 1.2);
			const float a = 1.0f - t * t;
			Print(a_v, Title(), std::format("+{}", a_st.lastPoints), a_v.w * 0.5f, a_v.h * 0.36f - 40.0f * t, 54, Fade(kBright, a), Fade(kGlow, a), 2.0f,
				Align::kCentre);
		}

		// the aim line (Atomic Rollers), as fo4bowling's: green, glowing, fading as it goes
		void Guide(ImGuiViewport* a_viewport, const View& a_v, const Arcade::HudState& a_st)
		{
			if (a_st.guide.size() < 2) {
				return;
			}
			const auto at = [&](const std::array<float, 2>& a_p) {
				return ImVec2{ a_viewport->Pos.x + a_p[0] * a_viewport->Size.x, a_viewport->Pos.y + a_p[1] * a_viewport->Size.y };
			};
			const float       scale = std::max(0.1f, a_viewport->Size.y / 720.0f);
			const std::size_t n = a_st.guide.size();
			for (std::size_t i = 1; i < n; ++i) {
				const float  fade = 1.0f - static_cast<float>(i) / static_cast<float>(n);
				const ImVec2 a = at(a_st.guide[i - 1]);
				const ImVec2 b = at(a_st.guide[i]);
				DL::AddLine(a_v.dl, a, b, Fade(kGlow, 0.45f * fade), 7.0f * scale);
				DL::AddLine(a_v.dl, a, b, Fade(kBright, 0.95f * fade + 0.05f), 2.5f * scale);
			}
		}

		// the end of a game: the score, the tickets out, a new best
		void Results(const View& a_v, const Arcade::HudState& a_st)
		{
			const float a = static_cast<float>(std::clamp(4.0 - a_st.resultsAge, 0.0, 1.0));
			const float w = 460.0f, h = a_st.newBest ? 176.0f : 150.0f, x = a_v.w * 0.5f - w * 0.5f, y = a_v.h * 0.3f;
			Chamfer(a_v, x, y, w, h, 14.0f, Fade(kPanel, a), Fade(kEdge, a));
			Print(a_v, Mono(), std::format("{} · GAME OVER", sim::GameName(a_st.game)), a_v.w * 0.5f, y + 14, 14, Fade(kDim, a), 0, 2.0f, Align::kCentre);
			Print(a_v, Title(), std::format("{} POINTS", Commas(a_st.score)), a_v.w * 0.5f, y + 38, 40, Fade(kBright, a), Fade(kGlow, a), 2.0f, Align::kCentre);
			Print(a_v, Title(), std::format("+{} TICKETS", a_st.won), a_v.w * 0.5f, y + 92, 30, Fade(kText, a), Fade(kGlow, a), 2.0f, Align::kCentre);
			if (a_st.newBest) {
				Print(a_v, Mono(), "A NEW BEST!", a_v.w * 0.5f, y + 140, 16, Fade(kBright, a), 0, 3.0f, Align::kCentre);
			}
		}

		void __stdcall Draw()
		{
			const auto st = Arcade::Get().Hud();
			auto*      viewport = GetMainViewport();
			if (!viewport || !(st.playing || st.results)) {
				return;
			}
			Texture(Title().file);
			if (g_artMissing) {
				return;
			}
			View v;
			v.dl = GetBackgroundDrawList();
			if (!v.dl) {
				return;
			}
			v.s = std::max(0.1f, viewport->Size.y / 720.0f);
			v.origin = viewport->Pos;
			v.w = viewport->Size.x / v.s;
			v.h = viewport->Size.y / v.s;
			if (st.results) {
				Results(v, st);
				return;
			}
			// while you play: the clock, and the power meter only while you're charging a throw (the machine's own
			// scoreboard has the score)
			Timer(v, st);
			Guide(viewport, v, st);
			if (st.info.showPoints) {
				Points(v, st);
			}
			if (st.info.power > 0.0) {
				Meter(v, st);
			}
		}

		F4SEMenuFramework::Model::HudElement* g_element{ nullptr };
	}

	bool Showing()
	{
		return g_element && !g_artMissing;
	}

	void Install()
	{
		if (g_element) {
			return;
		}
		if (!F4SEMenuFramework::IsInstalled()) {
			log::warn("F4SE Menu Framework not found: no HUD on screen (results come as notifications)");
			return;
		}
		g_element = F4SEMenuFramework::AddHudElement(Draw);
		log::info("HUD element registered");
	}
}
