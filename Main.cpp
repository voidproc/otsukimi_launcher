#include <Siv3D.hpp>


// フィールドのサイズ
const int W = 320;
const int H = 240;
const double SCALE = 2.0;


// ----------------------------------------
// テキスト描画
// ----------------------------------------

class ImageFont
{
public:
	ImageFont(const String& filename, const Size& charSize) : texture_(filename), charSize_(charSize)
	{
	}

	void draw(const String& text, const Vec2& pos, const Color& color = Palette::White, const double scale = 1.0, const int lineSpace = 1)
	{
		const SamplerState samplerState = Graphics2D::GetSamplerState();
		Graphics2D::SetSamplerState(SamplerState::ClampPoint);

		double x = 0;
		double y = 0;

		for (int i : step(text.length))
		{
			if (text[i] == L'\n')
			{
				x = 0;
				y += charSize_.y * scale + lineSpace;
			}
			else
			{
				texture_((text[i] % 16) * charSize_.x, (text[i] / 16) * charSize_.y, charSize_)
					.scale(scale).draw(pos.movedBy(x, y), color);

				x += charSize_.x * scale;
			}
		}

		Graphics2D::SetSamplerState(samplerState);
	}

	void drawAt(const String& text, const Vec2& pos, const Color& color = Palette::White, const double scale = 1.0, const int lineSpace = 1)
	{
		const SamplerState samplerState = Graphics2D::GetSamplerState();
		Graphics2D::SetSamplerState(SamplerState::ClampPoint);

		// Calculate region size

		int maxLineLen = 0;
		int lines = 1 + text.count(L'\n');

		if (lines > 1)
		{
			size_t i;
			int lastFound = 0;

			while (true)
			{
				i = text.indexOf(L'\n', lastFound);
				if (i == String::npos) { break; }

				maxLineLen = Max(maxLineLen, (int)i - lastFound);
			}
		}
		else
		{
			maxLineLen = text.length;
		}

		Size regionSize(maxLineLen * charSize_.x * scale, lines * (charSize_.y * scale + lineSpace));

		// Draw

		double x = 0;
		double y = 0;

		for (int i : step(text.length))
		{
			if (text[i] == L'\n')
			{
				x = 0;
				y += charSize_.y * scale + lineSpace;
			}
			else
			{
				texture_((text[i] % 16) * charSize_.x, (text[i] / 16) * charSize_.y, charSize_)
					.scale(scale).draw(pos.movedBy(x - regionSize.x / 2, y - regionSize.y / 2), color);

				x += charSize_.x * scale;
			}
		}

		Graphics2D::SetSamplerState(samplerState);
	}

private:
	Texture texture_;
	Size charSize_;
};


// ----------------------------------------
// 次のキャラクタ生成までの時間
// ----------------------------------------

const int nextEnemyGenFrame(const int frame)
{
	const int t = Max(30, 60 - (frame / 250));
	return frame + Random(t, t + Max(10, 30 - (frame / 320)));
}

const int nextItemGenFrame(const int frame)
{
	const int t = Max(30, 60 - (frame / 700));
	return frame + Random(t, t + Max(10, 30 - (frame / 1000)));
}


// ----------------------------------------
// 月の位置
// ----------------------------------------

const double moonHeight(const int frame)
{
	return 30 + frame / 40;
}


// ----------------------------------------
// プレイヤーの状態
// ----------------------------------------

struct Player
{
	Vec2 pos{ 100, 200 };
	bool right = true;
	Circle collision{ 0, 3, 8 };
	int odango = 0;
	int usa = 0;
	int damage = 0;
	int score = 0;
	int life = 3;
	int itemget = 0;
};


// ----------------------------------------
// 敵・アイテムの状態
// ----------------------------------------

struct Character
{
	String name;
	Vec2 pos;
	Circular vel;
	int frame = 0;
	bool alive = true;
	Circle collision{ 0, 0, 1 };

	Character(const String name = L"", const Vec2& pos = { 0, 0 }, const Circular& vel = { 0, 0 })
		: name(name), pos(pos), vel(vel)
	{
		if (name == L"enemy1")
		{
			collision.r = 12;
		}
		else if (name == L"item1")
		{
			collision.r = 20;
		}
		else if (name == L"usa")
		{
			collision.r = 20;
			this->vel = Circular(Vec2(0, 1));
		}
	}

	void update()
	{
		if (name == L"enemy1")
		{
			pos += vel;
		}
		else if (name == L"item1")
		{
			Vec2 v = vel;
			v.y += 0.05;
			vel = v;
			pos.x += v.x;
			pos.y += v.y;
		}
		else if (name == L"usa")
		{
			Vec2 v = vel;
			v.x = 2.0 * sin(frame / 20.0);
			pos.x += v.x;
			pos.y += v.y;
		}

		++frame;
	}

	void draw()
	{
		if (name == L"enemy1")
		{
			Circle(pos, 15).draw(Color(200, 0, 0 , 80 * ((frame / 2) % 2)));
			TextureAsset(L"stone").rotate(Math::HalfPi * ((frame / 15) % 4)).drawAt(pos);
		}
		else if (name == L"item1")
		{
			Circle(pos, 20).draw(Color(255, 255, 255, 40 + 40 * ((frame / 2) % 2)));
			TextureAsset(L"odango").drawAt(pos);
		}
		else if (name == L"usa")
		{
			Circle(pos, 20).draw(Color(255, 255, 128, 40 + 60 * ((frame / 2) % 2)));
			TextureAsset(L"usa").drawAt(pos);
		}
	}
};


// ----------------------------------------
// 爆発エフェクト
// ----------------------------------------

struct Explode : public IEffect
{
	Explode(const Vec2& pos, const Color& color)
		: pos(pos), rot(Random(0, 3) * Math::HalfPi), t_(Random(0.2, 1.0)), scale(Random(1.0, 2.5)), color(color)
	{
	}

	bool update(double t) override
	{
		TextureAsset(L"explode")(16 * Min((int)(t/t_ * 6), 5), 0, 16, 16).scale(scale).rotate(rot).drawAt(pos, color);
		return t < t_;
	}

	Vec2 pos;
	double rot;
	double t_;
	double scale;
	Color color;
};


// ----------------------------------------
// 取得スコア表示エフェクト
// ----------------------------------------

struct Score : public IEffect
{
	Score(ImageFont* font, const Vec2& pos, const String& type, const int pts, const int ratio): font(font), pos(pos), type(type), pts(pts), ratio(ratio)
	{
	}

	bool update(double t) override
	{
		if (type == L"moon")
		{
			font->drawAt(Format(pts, L"*", ratio), pos.movedBy(-8 * t, 0), Color(255, 255 * ((System::FrameCount() / 3) % 2), 0), 2);
		}
		else
		{
			font->drawAt(Format(pts), pos.movedBy(0, -16 * t), Color(255, 200 + 55*((System::FrameCount() / 3) % 2), 0), 1);
		}
		return t < 1.0;
	}

	ImageFont* font;
	Vec2 pos;
	String type;
	int pts;
	int ratio;
};


// ----------------------------------------
// キラキラ
// ----------------------------------------

struct Kirakira : public IEffect
{
	Kirakira(const Vec2& pos) : pos(pos)
	{
	}

	bool update(double t) override
	{
		TextureAsset(L"kirakira")(16 * Min((int)(t/0.3 * 3), 2), 0, 16, 16).drawAt(pos);
		return t < 0.3;
	}

	Vec2 pos;
};


// ----------------------------------------
// ロケット
// 打ち上がって月へ着陸する
// ----------------------------------------

struct Rocket
{
	Rocket() {}

	Rocket(const Vec2& pos, Player& player, Effect* effect) : pos(pos), effect(effect), targetx(Random(-20, 20))
	{
		odango = player.odango;
		player.odango = 0;
		usa = player.usa;
		player.usa = 0;
	}

	void update(const int gameFrame)
	{
		if (InRange(frame, 0, 59))
		{
			// 震える
			TextureAsset(L"rocket").drawAt(pos + RandomVec2({ -4, 4 }, { -4, 4 }));
			Circle(pos.movedBy(RandomVec2({ -8, 8 }, { -8, 8 }) + Vec2(0, 40)), Random(4.0, 12.0)).draw(Palette::Orange);
			Circle(pos, 8 + frame * 8).drawFrame(1, 0, Alpha(255 - frame * 3));
		}
		else if (InRange(frame, 60, 179))
		{
			// 打ち上がり中
			current = pos + Vec2(0, -(pos.y+20) * EaseIn<Easing::Cubic>((frame - 60) / 120.0));
			TextureAsset(L"rocket").drawAt(current);
			Circle(current.movedBy(RandomVec2({ -8, 8 }, { -8, 8 }) + Vec2(0, 40)), Random(4.0, 12.0)).draw(Palette::Orange);
		}
		else if (InRange(frame, 180, 299))
		{
			// 月へ移動
			const Vec2 pos0{ pos.x, -10 };
			const Vec2 moon{ W / 2 + targetx, moonHeight(gameFrame) - 30 };
			const Circular dir = moon - pos0;

			current = EaseInOut<Easing::Sine>(pos0, moon, (frame - 180) / 120.0);
			TextureAsset(L"rocket").scale(0.3).rotate(dir.theta).drawAt(current, Palette::Black);
			Circle(current.movedBy(RandomVec2({ -2, 2 }, { -2, 2 }) - Circular(12, dir.theta)), Random(1.2, 3.6)).draw(Color(200, 120, 0));
		}
		else if (InRange(frame, 300, 330))
		{
			// 爆発
			if (frame % 5 == 0)
			{
				Vec2 v = Vec2(W / 2 + targetx, moonHeight(gameFrame) - 30) + RandomVec2({ -30, 30 }, { -30, 30 });
				v.y = Clamp(v.y, 0.0, H-80-20.0);
				effect->add<Explode>(v, Palette::Black);
			}
		}

		++frame;
	}

	Vec2 pos, current;
	double targetx;
	int frame = 0;
	int odango = 0;
	int usa = 0;
	Effect* effect;
};


void Main()
{
	Window::SetTitle(L"(;x;) - Siv3D Game Jam 第15回 テーマ「月」投稿作品");
	Window::Resize(W * SCALE, H * SCALE);

	Graphics::SetBackground(Palette::Black);
	Graphics2D::SetSamplerState(SamplerState::WrapPoint);
	
	RenderTexture renderTexture(W, H, Palette::Darkblue);

	TextureAsset::Register(L"player", L"Assets/player.png");
	TextureAsset::Register(L"moon", L"Assets/moon.png");
	TextureAsset::Register(L"bg", L"Assets/bg.png");
	TextureAsset::Register(L"sky", L"Assets/sky.png");
	TextureAsset::Register(L"stone", L"Assets/stone2.png");
	TextureAsset::Register(L"odango", L"Assets/odango.png");
	TextureAsset::Register(L"usa", L"Assets/item2.png");
	TextureAsset::Register(L"rocket", L"Assets/rocket.png");
	TextureAsset::Register(L"explode", L"Assets/explode.png");
	TextureAsset::Register(L"kirakira", L"Assets/kirakira.png");
	TextureAsset::Register(L"heart", L"Assets/heart.png");
	TextureAsset::Register(L"title", L"Assets/title.png");
	TextureAsset::Register(L"titles", L"Assets/titles.png");

	ImageFont imageFont(L"Assets/font.png", {8, 8});

	Effect effect;

	Player player;
	Array<Rocket> rocket;
	Array<Character> characters;

	int frame = 0;
	int enemyGenFrame = nextEnemyGenFrame(frame);
	int itemGenFrame = nextItemGenFrame(frame);

	String scene = L"title";


	while (System::Update())
	{
		Graphics2D::SetRenderTarget(renderTexture);

		if (scene == L"title")
		{
			// ----------------------------------------
			// ◆ タイトル
			// ----------------------------------------

			Rect(W, H).draw(Color(0, 20, 80));
			TextureAsset(L"moon").scale(2).drawAt(W / 2, H / 2 - 10);
			Rect(W, H).draw(Color(0, 0, 20, 150));
			TextureAsset(L"titles").drawAt(W / 2, H / 2 - 10, Color(0, 128 * ((frame)%2)));
			TextureAsset(L"title").drawAt(W / 2, H / 2 - 10);

			imageFont.drawAt(L"PRESS ENTER KEY", Vec2(W / 2, H - 40), Color(128+127*((frame/2)%2)));

			if (Input::KeyEnter.clicked)
			{
				scene = L"main";
				frame = 0;

				// メインのシーンをリセット
				player = Player();
				enemyGenFrame = nextEnemyGenFrame(0);
				itemGenFrame = nextItemGenFrame(0);
				rocket.clear();
				characters.clear();
			}
		}
		else if (scene == L"main")
		{
			// ----------------------------------------
			// 背景など
			// ----------------------------------------

			TextureAsset(L"sky").map(W, H - 80).draw();
			TextureAsset(L"moon").drawAt(W / 2, moonHeight(frame), Color(255, Max(0, 255 - frame / 30), Max(0, 255 - frame / 30)));
			TextureAsset(L"bg").draw();


			// ----------------------------------------
			// プレイヤー
			// ----------------------------------------

			// 歩く
			// 荷物がたくさんあると足が遅くなる

			double walkSpeed = Max(0.5, 3.0 - 0.05 * player.odango - 0.2 * player.usa);
			if (player.damage > 0) walkSpeed = 0;

			int animSpeed = 25;

			if (Input::KeyLeft.pressed)
			{
				player.pos.moveBy(-walkSpeed, 0);
				player.right = false;
				animSpeed = 10;
			}
			if (Input::KeyRight.pressed)
			{
				player.pos.moveBy(walkSpeed, 0);
				player.right = true;
				animSpeed = 10;
			}

			player.pos = Clamp(player.pos, { 8, 8 }, { W - 8, H - 8 });

			// ロケット打ち上げ
			if (Input::KeySpace.clicked)
			{
				if (player.odango > 0 && player.usa > 0)
				{
					rocket.emplace_back(player.pos.movedBy(0, -30), player, &effect);
					player.damage = 30;
				}
			}

			player.damage = Max(player.damage - 1, 0);


			// ----------------------------------------
			// ロケット
			// ----------------------------------------

			for (auto& r : rocket)
			{
				r.update(frame);
			}

			Erase_if(rocket, [&](auto& r) {
				const bool ret = r.frame > 330;
				if (ret)
				{
					player.score += r.odango * 500 * Min<int>(Pow(2, r.usa), 64);
					effect.add<Score>(&imageFont, r.current, L"moon", r.odango * 500, Min<int>(Pow(2, r.usa), 64));
				}
				return ret;
			});


			// ----------------------------------------
			// 敵・アイテム
			// ----------------------------------------

			if (frame == enemyGenFrame)
			{
				characters.emplace_back(
					L"enemy1",
					Vec2(Random((double)W), -32.0),
					Circular(Random(1.5, 3.0), Random(160_deg, 200_deg)));

				enemyGenFrame = nextEnemyGenFrame(frame);
			}

			if (frame == itemGenFrame)
			{
				if (RandomBool(0.7))
				{
					Vec2 pos{ RandomSelect({ -32, W + 32 }), Random(50, 100) };
					characters.emplace_back(
						L"item1",
						pos,
						Circular(Random(1.5, 3.0), Random(30_deg, 90_deg) * (pos.x > 0 ? -1 : 1)));
				}
				else {
					characters.emplace_back(L"usa", Vec2(Random(20, W - 20), -32));
				}

				itemGenFrame = nextItemGenFrame(frame);
			}

			for (auto& c : characters)
			{
				c.update();
			}


			// ----------------------------------------
			// プレイヤーと敵・アイテムの衝突
			// ----------------------------------------

			Erase_if(characters, [&](auto& c) {
				const bool ret = player.collision.movedBy(player.pos).intersects(c.collision.movedBy(c.pos));
				if (ret)
				{
					if (c.name == L"item1")
					{
						player.odango += 1;
						player.score += 100;
						effect.add<Score>(&imageFont, player.pos.movedBy(0, -10), L"", 100, 0);
						player.itemget = 30;
					}
					else if (c.name == L"usa")
					{
						player.usa = Min(6, player.usa + 1);
						player.score += 500;
						effect.add<Score>(&imageFont, player.pos.movedBy(0, -10), L"", 500, 0);
						player.itemget = 30;
					}
					else if (c.name == L"enemy1")
					{
						// 点滅中はぶつからない
						if (player.damage > 0) return false;

						// Damaged
						--player.life;
						player.damage = 60;

						// Explode fx
						for (int i : step(8))
						{
							effect.add<Explode>(c.pos + RandomVec2({ -30, 30 }, { -30, 30 }), Palette::White);
						}
					}
				}
				return ret;
			});

			Erase_if(characters, [](auto& c) { return !Rect(W, H).stretched(32).intersects(c.pos); });


			// キラキラ
			if (player.itemget > 0)
			{
				if (frame % 5 == 0)
					effect.add<Kirakira>(player.pos + RandomVec2({ -16, 16 }, { -16, 16 }));
				--player.itemget;
			}


			// ----------------------------------------
			// 描画
			// ----------------------------------------

			// Player
			Color color = player.damage > 0 ? Alpha(((frame / 3) % 2) * 255) : Alpha(255);
			if (player.odango > 0 && player.usa > 0)
			{
				color.r = color.b = 255 - 100 * ((frame / 3) % 2);
				color.g = 255;
			}

			if (player.right)
			{
				Ellipse(player.pos.movedBy(-4, 16), 24, 6).draw(Color(Palette::Black, 128 - 128*((frame/2)%2))); //shadow
				TextureAsset(L"player")(((frame / animSpeed) % 2) * 48, 0, 48, 32).drawAt(player.pos, color);
			}
			else
			{
				Ellipse(player.pos.movedBy(4, 16), 24, 6).draw(Color(Palette::Black, 128 - 128*((frame/2)%2))); //shadow
				TextureAsset(L"player")(((frame / animSpeed) % 2) * 48, 0, 48, 32).mirror().drawAt(player.pos, color);
			}

			// Effect
			effect.update();

			// Enemy, Item
			for (auto& c : characters)
			{
				c.draw();
			}

			// Status
			for (int i : step(player.life))
			{
				TextureAsset(L"heart")(16 * ((frame / 15) % 2), 0, 16, 16).draw(i * 14, 0);
			}
			imageFont.drawAt(Pad(player.score, { 10, L'0' }), { W / 2, 8 });
			imageFont.draw(Pad(((190 - 30) * 40 - frame) / 60, { 3, L' ' }), { W - 2 * (3 * 8), 2 }, Palette::Blue, 2);


			// ----------------------------------------
			// シーンチェンジ
			// ----------------------------------------

			if (player.life <= 0)
			{
				scene = L"over";
				frame = 0;
			}

			if (moonHeight(frame) == 190)
			{
				scene = L"over";
				frame = 0;
			}
		}
		else if (scene == L"over")
		{
			// ----------------------------------------
			// ◆ ゲームオーバー
			// ----------------------------------------

			Rect(W, H).draw(Color(20, 20, 40, 5));

			imageFont.drawAt(L"OTSUKIMI OVER", Vec2(W / 2, H / 2 - 20), Palette::Red, 2);
			imageFont.drawAt(Format(L"SCORE: ", player.score), Vec2(W / 2, H / 2 + 20), Palette::White, 2);

			if (frame > 120)
			{
				imageFont.drawAt(L"PRESS ENTER KEY", Vec2(W / 2, H - 40), Color(128+127*((frame/2)%2)));

				if (Input::KeyEnter.clicked)
				{
					scene = L"title";
					frame = 0;
				}
			}
		}


		Graphics2D::SetRenderTarget(Graphics::GetSwapChainTexture());
		renderTexture.scale(SCALE).draw();

		++frame;
	}
}
