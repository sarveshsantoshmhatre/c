#include "rng.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int W = 44;
constexpr int H = 22;
constexpr int MAX_INV = 18;

struct Pos { int x=0, y=0; };

bool same(Pos a, Pos b) { return a.x == b.x && a.y == b.y; }
int dist(Pos a, Pos b) { return std::abs(a.x-b.x) + std::abs(a.y-b.y); }

enum class ItemKind { Potion, Bomb, Sword, Armor, Scrap, Crystal, Key, Food };

struct Item {
    ItemKind kind;
    std::string name;
    int power = 0;
    int value = 0;
    bool equipped = false;
};

enum class EnemyKind { Rat, Goblin, Archer, Brute, Warden };

struct Enemy {
    Pos p;
    EnemyKind kind;
    std::string name;
    char glyph = 'e';
    int hp = 1, maxHp = 1, attack = 1, defense = 0;
    int xp = 1, gold = 0, range = 1;
    bool alive = true;
};

struct Quest {
    int stage = 0;
    int kills = 0;
    int crystals = 0;
    bool complete = false;
};

struct Player {
    Pos p{2,2};
    int hp=30, maxHp=30;
    int mana=8, maxMana=8;
    int attack=5, defense=1;
    int level=1, xp=0, nextXp=25;
    int gold=15;
    int floor=1;
    int turns=0;
    int potionsUsed=0;
    std::vector<Item> inv;
    Quest quest;
};

class Game {
public:
    explicit Game(uint64_t seed) : seed_(seed) {
        sd_rng_seed(&rng_, seed_);
        generate();
        log("You enter the Shattered Depths. Find the relic and survive.");
    }

    void run() {
        while (running_) {
            render();
            std::cout << "\nCommand [wasd/f/g/i/e/c/q/>/S/L/?/x]: ";
            std::string cmd;
            if (!std::getline(std::cin, cmd)) break;
            if (cmd.empty()) continue;
            handle(cmd[0]);
        }
        std::cout << "\nRun ended. Seed: " << seed_ << "\n";
    }

private:
    uint64_t seed_;
    sd_rng rng_{};
    Player player_;
    Quest quest_;
    std::array<std::string,H> map_{};
    std::array<std::array<bool,W>,H> seen_{};
    std::vector<Enemy> enemies_;
    std::vector<std::pair<Pos,Item>> ground_;
    std::vector<std::pair<Pos,int>> traps_;
    Pos stairs_{W-3,H-3};
    Pos shrine_{W/2,H/2};
    std::string message_;
    bool running_ = true;
    int score_ = 0;

    int r(int a,int b) { return sd_rng_range(&rng_,a,b); }
    bool chance(int p) { return sd_rng_chance(&rng_,p) != 0; }

    void clearMap() {
        for (auto& row : map_) row = std::string(W, '#');
        for (auto& row : seen_) row.fill(false);
        enemies_.clear();
        ground_.clear();
        traps_.clear();
    }

    bool inside(Pos p) const {
        return p.x>0 && p.x<W-1 && p.y>0 && p.y<H-1;
    }

    bool floorAt(Pos p) const {
        return inside(p) && map_[p.y][p.x] == '.';
    }

    void carveRoom(int x1,int y1,int x2,int y2) {
        for (int y=y1;y<=y2;y++)
            for (int x=x1;x<=x2;x++)
                if (x>0 && x<W-1 && y>0 && y<H-1) map_[y][x]='.';
    }

    void carveH(int x1,int x2,int y) {
        if (x1>x2) std::swap(x1,x2);
        for (int x=x1;x<=x2;x++) if (x>0&&x<W-1&&y>0&&y<H-1) map_[y][x]='.';
    }

    void carveV(int y1,int y2,int x) {
        if (y1>y2) std::swap(y1,y2);
        for (int y=y1;y<=y2;y++) if (x>0&&x<W-1&&y>0&&y<H-1) map_[y][x]='.';
    }

    void generate() {
        clearMap();
        std::vector<Pos> centers;
        for (int i=0;i<10;i++) {
            int rw=r(4,8), rh=r(3,5);
            int x=r(2,W-rw-3), y=r(2,H-rh-3);
            carveRoom(x,y,x+rw,y+rh);
            Pos c{x+rw/2,y+rh/2};
            if (!centers.empty()) {
                Pos prev=centers.back();
                carveH(prev.x,c.x,prev.y);
                carveV(prev.y,c.y,c.x);
            }
            centers.push_back(c);
        }

        player_.p = centers.front();
        stairs_ = centers.back();
        shrine_ = centers[r(2,(int)centers.size()-2)];

        for (int i=0;i<7+player_.floor*2;i++) {
            Pos p=randomFloorFarFrom(player_.p,7);
            if (p.x<0) continue;
            Enemy e = makeEnemy();
            e.p=p;
            enemies_.push_back(e);
        }

        for (int i=0;i<5+player_.floor;i++) {
            Pos p=randomFloorFarFrom(player_.p,4);
            if (p.x<0) continue;
            ground_.push_back({p, randomItem()});
        }

        for (int i=0;i<3+player_.floor/2;i++) {
            Pos p=randomFloorFarFrom(player_.p,5);
            if (p.x<0) continue;
            traps_.push_back({p,r(2,7)});
        }

        if (player_.floor == 3) {
            Enemy w;
            w.p = stairs_;
            w.kind=EnemyKind::Warden;
            w.name="Depth Warden"; w.glyph='W';
            w.maxHp=w.hp=75; w.attack=11; w.defense=5; w.xp=100; w.gold=100; w.range=1;
            enemies_.push_back(w);
        }

        if (player_.floor == 1 && player_.quest.stage == 0)
            ground_.push_back({centers[centers.size()/2], Item{ItemKind::Key,"Ancient Key",0,30,false}});

        reveal();
    }

    Pos randomFloorFarFrom(Pos origin,int minDist) {
        for (int tries=0;tries<80;tries++) {
            Pos p{r(1,W-2),r(1,H-2)};
            if (floorAt(p) && dist(p,origin)>=minDist && !enemyAt(p) && !groundAt(p))
                return p;
        }
        return {-1,-1};
    }

    Item randomItem() {
        int n=r(1,100);
        if (n<=28) return {ItemKind::Potion,"Health Potion",12,12,false};
        if (n<=40) return {ItemKind::Food,"Ration",7,5,false};
        if (n<=50) return {ItemKind::Scrap,"Scrap",0,4,false};
        if (n<=62) return {ItemKind::Crystal,"Void Crystal",0,25,false};
        if (n<=73) return {ItemKind::Bomb,"Arc Bomb",18,20,false};
        if (n<=87) return {ItemKind::Sword,"Tempered Blade",3,45,false};
        return {ItemKind::Armor,"Runic Armor",2,45,false};
    }

    Enemy makeEnemy() {
        int n=r(1,100);
        Enemy e;
        if (n<=38) {
            e.kind=EnemyKind::Rat; e.name="Cave Rat"; e.glyph='r';
            e.maxHp=e.hp=7+player_.floor; e.attack=3+player_.floor; e.defense=0; e.xp=5; e.gold=r(1,5);
        } else if (n<=65) {
            e.kind=EnemyKind::Goblin; e.name="Goblin Raider"; e.glyph='g';
            e.maxHp=e.hp=13+player_.floor*2; e.attack=5+player_.floor; e.defense=1; e.xp=9; e.gold=r(4,10);
        } else if (n<=84) {
            e.kind=EnemyKind::Archer; e.name="Ash Archer"; e.glyph='a';
            e.maxHp=e.hp=10+player_.floor*2; e.attack=6+player_.floor; e.defense=1; e.xp=12; e.gold=r(5,12); e.range=5;
        } else {
            e.kind=EnemyKind::Brute; e.name="Stone Brute"; e.glyph='B';
            e.maxHp=e.hp=24+player_.floor*5; e.attack=8+player_.floor*2; e.defense=3; e.xp=20; e.gold=r(10,20);
        }
        return e;
    }

    bool enemyAt(Pos p) const {
        for (const auto& e:enemies_) if (e.alive && same(e.p,p)) return true;
        return false;
    }

    int enemyIndexAt(Pos p) const {
        for (int i=0;i<(int)enemies_.size();i++)
            if (enemies_[i].alive && same(enemies_[i].p,p)) return i;
        return -1;
    }

    bool groundAt(Pos p) const {
        for (const auto& g:ground_) if (same(g.first,p)) return true;
        return false;
    }

    bool blocked(Pos p) const {
        return !floorAt(p) || enemyAt(p) || same(p,stairs_) || same(p,shrine_);
    }

    void reveal() {
        for (int y=0;y<H;y++) for (int x=0;x<W;x++) {
            Pos p{x,y};
            if (dist(player_.p,p)<=6) seen_[y][x]=true;
        }
    }

    char tile(Pos p) const {
        if (same(p,player_.p)) return '@';
        int ei=enemyIndexAt(p);
        if (ei>=0) return enemies_[ei].glyph;
        if (same(p,stairs_)) return '>';
        if (same(p,shrine_)) return '+';
        for (const auto& t:traps_) if (same(t.first,p)) return seen_[p.y][p.x]?'^':'.';
        for (const auto& g:ground_) if (same(g.first,p)) return '$';
        return map_[p.y][p.x];
    }

    void render() {
        std::cout << "\033[2J\033[H";
        std::cout << "=== THE SHATTERED DEPTHS ===  Floor " << player_.floor
                  << "  Turn " << player_.turns << "  Gold " << player_.gold
                  << "  Score " << score_ << "\n";
        std::cout << "HP " << player_.hp << "/" << player_.maxHp
                  << "  MP " << player_.mana << "/" << player_.maxMana
                  << "  LV " << player_.level << " XP " << player_.xp << "/" << player_.nextXp
                  << "  ATK " << player_.attack << " DEF " << player_.defense << "\n";

        for (int y=0;y<H;y++) {
            for (int x=0;x<W;x++) {
                Pos p{x,y};
                if (!seen_[y][x]) { std::cout << ' '; continue; }
                char c=tile(p);
                if (same(p,player_.p)) std::cout << "\033[1;36m" << c << "\033[0m";
                else if (c=='W' || c=='B') std::cout << "\033[1;31m" << c << "\033[0m";
                else if (c=='$') std::cout << "\033[1;33m" << c << "\033[0m";
                else if (c=='>') std::cout << "\033[1;35m" << c << "\033[0m";
                else if (c=='+') std::cout << "\033[1;32m" << c << "\033[0m";
                else std::cout << c;
            }
            std::cout << "\n";
        }
        std::cout << "Quest: " << questText() << "\n";
        std::cout << "Log: " << message_ << "\n";
    }

    std::string questText() const {
        if (quest_.complete) return "The relic is claimed. Defeat the Warden.";
        if (quest_.stage==0) return "Find the Ancient Key.";
        if (quest_.stage==1) return "Collect 3 Void Crystals ("+std::to_string(quest_.crystals)+"/3).";
        return "Reach the Depth Warden and defeat it.";
    }

    void handle(char c) {
        switch(c) {
            case 'w': move(0,-1); break;
            case 's': move(0,1); break;
            case 'a': move(-1,0); break;
            case 'd': move(1,0); break;
            case 'f': attackNearest(); break;
            case 'g': pickup(); break;
            case 'i': inventory(); return;
            case 'e': useOrEquip(); return;
            case 'c': craft(); break;
            case 'q': questScreen(); return;
            case '>': descend(); break;
            case 'S': save(); return;
            case 'L': load(); return;
            case '?': help(); return;
            case 'x': running_=false; return;
            default: message_="Unknown command. Press ? for help."; return;
        }
        if (c!='x' && c!='i' && c!='e' && c!='q' && c!='S' && c!='L' && c!='?') {
            player_.turns++;
            enemyTurn();
            reveal();
            checkTraps();
            checkQuest();
            if (player_.hp<=0) {
                render();
                std::cout << "\nYou were consumed by the Depths. Final score: " << score_ << "\n";
                running_=false;
            }
        }
    }

    void move(int dx,int dy) {
        Pos np{player_.p.x+dx,player_.p.y+dy};
        int ei=enemyIndexAt(np);
        if (ei>=0) {
            melee(ei);
            return;
        }
        if (!floorAt(np)) { message_="A wall blocks your path."; return; }
        if (same(np,stairs_)) { message_="The stairs descend here. Press >."; return; }
        if (same(np,shrine_)) {
            player_.p=np;
            player_.hp=player_.maxHp;
            player_.mana=player_.maxMana;
            message_="The shrine restores your body and mind.";
            return;
        }
        player_.p=np;
        message_="You move through the ruins.";
        for (const auto& g:ground_) if (same(g.first,np)) message_="Loot is here. Press g.";
    }

    void melee(int ei) {
        Enemy& e=enemies_[ei];
        int base=player_.attack+r(0,3)-e.defense;
        bool crit=chance(15+player_.level*2);
        int dmg=std::max(1,base)*(crit?2:1);
        e.hp-=dmg;
        message_=(crit?"Critical strike! ":"")+"You hit "+e.name+" for "+std::to_string(dmg)+".";
        if (e.hp<=0) killEnemy(ei);
    }

    void attackNearest() {
        int best=-1,bestd=999;
        for (int i=0;i<(int)enemies_.size();i++) if (enemies_[i].alive) {
            int d=dist(player_.p,enemies_[i].p);
            if (d<=1 && d<bestd) { best=i;bestd=d; }
        }
        if (best<0) { message_="No adjacent enemy to attack."; return; }
        melee(best);
    }

    void killEnemy(int i) {
        Enemy& e=enemies_[i];
        e.alive=false;
        player_.xp+=e.xp;
        player_.gold+=e.gold;
        score_+=e.xp*10+e.gold;
        quest_.kills++;
        message_ += " "+e.name+" falls. +"+std::to_string(e.xp)+" XP.";
        if (chance(35)) ground_.push_back({e.p,randomItem()});
        levelUp();
    }

    void levelUp() {
        while (player_.xp>=player_.nextXp) {
            player_.xp-=player_.nextXp;
            player_.level++;
            player_.nextXp=20+player_.level*18;
            player_.maxHp+=6;
            player_.hp=player_.maxHp;
            player_.maxMana++;
            player_.mana=player_.maxMana;
            player_.attack+=2;
            if (player_.level%2==0) player_.defense++;
            message_+=" LEVEL UP! You are now level "+std::to_string(player_.level)+".";
        }
    }

    void pickup() {
        for (auto it=ground_.begin();it!=ground_.end();++it) {
            if (same(it->first,player_.p)) {
                if ((int)player_.inv.size()>=MAX_INV) { message_="Inventory full."; return; }
                Item item=it->second;
                if (item.kind==ItemKind::Crystal) quest_.crystals++;
                message_="Picked up "+item.name+".";
                player_.inv.push_back(item);
                ground_.erase(it);
                checkQuest();
                return;
            }
        }
        message_="There is nothing to pick up.";
    }

    void inventory() {
        std::cout << "\nInventory ("<<player_.inv.size()<<"/"<<MAX_INV<<")\n";
        if (player_.inv.empty()) std::cout << "  Empty\n";
        for (size_t i=0;i<player_.inv.size();i++)
            std::cout << "  "<<i+1<<". "<<player_.inv[i].name
                      <<(player_.inv[i].equipped?" [E]":"")<<"\n";
        std::cout << "Press Enter.";
        std::string s; std::getline(std::cin,s);
    }

    void useOrEquip() {
        inventory();
        std::cout << "Item number to use/equip (0 cancel): ";
        std::string s; std::getline(std::cin,s);
        int n=std::stoi(s.empty()?"0":s);
        if (n<=0 || n>(int)player_.inv.size()) { message_="Cancelled."; return; }
        Item &item=player_.inv[n-1];
        if (item.kind==ItemKind::Potion) {
            player_.hp=std::min(player_.maxHp,player_.hp+item.power);
            player_.potionsUsed++;
            message_="You drink a Health Potion.";
            player_.inv.erase(player_.inv.begin()+n-1);
        } else if (item.kind==ItemKind::Food) {
            player_.hp=std::min(player_.maxHp,player_.hp+item.power);
            message_="You eat a ration.";
            player_.inv.erase(player_.inv.begin()+n-1);
        } else if (item.kind==ItemKind::Bomb) {
            int target=-1,bd=99;
            for (int i=0;i<(int)enemies_.size();i++) if (enemies_[i].alive) {
                int d=dist(player_.p,enemies_[i].p);
                if (d<=3 && d<bd) {target=i;bd=d;}
            }
            if (target<0) { message_="No enemy is close enough for the Arc Bomb."; return; }
            enemies_[target].hp-=item.power;
            message_="The Arc Bomb detonates for "+std::to_string(item.power)+" damage.";
            if (enemies_[target].hp<=0) killEnemy(target);
            player_.inv.erase(player_.inv.begin()+n-1);
        } else if (item.kind==ItemKind::Sword) {
            for(auto& x:player_.inv) if(x.kind==ItemKind::Sword) x.equipped=false;
            item.equipped=true;
            player_.attack=5+item.power+(player_.level-1)*2;
            message_="Equipped "+item.name+".";
        } else if (item.kind==ItemKind::Armor) {
            for(auto& x:player_.inv) if(x.kind==ItemKind::Armor) x.equipped=false;
            item.equipped=true;
            player_.defense=1+item.power+(player_.level/2);
            message_="Equipped "+item.name+".";
        } else {
            message_="That item cannot be used directly.";
        }
    }

    void craft() {
        int scrap=0;
        for(const auto& x:player_.inv) if(x.kind==ItemKind::Scrap) scrap++;
        if (scrap<2) { message_="Crafting requires 2 Scrap."; return; }
        if ((int)player_.inv.size()>=MAX_INV) { message_="Inventory full."; return; }
        int removed=0;
        for(auto it=player_.inv.begin();it!=player_.inv.end() && removed<2;) {
            if(it->kind==ItemKind::Scrap) { it=player_.inv.erase(it); removed++; }
            else ++it;
        }
        player_.inv.push_back({ItemKind::Potion,"Crafted Medkit",18,15,false});
        message_="You craft a powerful Medkit from 2 Scrap.";
    }

    void enemyTurn() {
        for (auto& e:enemies_) {
            if (!e.alive) continue;
            int d=dist(e.p,player_.p);
            if (e.range>1 && d<=e.range && lineOfSight(e.p,player_.p)) {
                int dmg=std::max(1,e.attack+r(0,2)-player_.defense);
                player_.hp-=dmg;
                message_+=" "+e.name+" shoots for "+std::to_string(dmg)+".";
                continue;
            }
            if (d==1) {
                int dmg=std::max(1,e.attack+r(0,2)-player_.defense);
                if (chance(10)) dmg*=2;
                player_.hp-=dmg;
                message_+=" "+e.name+" hits you for "+std::to_string(dmg)+".";
                continue;
            }
            if (d<=8) {
                Pos step=e.p;
                int dx=player_.p.x-e.p.x, dy=player_.p.y-e.p.y;
                if (std::abs(dx)>=std::abs(dy)) step.x+=(dx>0?1:-1);
                else step.y+=(dy>0?1:-1);
                if (floorAt(step) && !enemyAt(step) && !same(step,stairs_) && !same(step,shrine_))
                    e.p=step;
            } else if (chance(12)) {
                Pos np{e.p.x+r(-1,1),e.p.y+r(-1,1)};
                if (floorAt(np) && !enemyAt(np)) e.p=np;
            }
        }
    }

    bool lineOfSight(Pos a,Pos b) const {
        if (a.x==b.x) {
            int y1=std::min(a.y,b.y), y2=std::max(a.y,b.y);
            for(int y=y1+1;y<y2;y++) if(map_[y][a.x]=='#') return false;
            return true;
        }
        if (a.y==b.y) {
            int x1=std::min(a.x,b.x), x2=std::max(a.x,b.x);
            for(int x=x1+1;x<x2;x++) if(map_[a.y][x]=='#') return false;
            return true;
        }
        return false;
    }

    void checkTraps() {
        for (auto& t:traps_) if(same(t.first,player_.p)) {
            int dmg=t.second;
            player_.hp-=dmg;
            message_="A hidden trap triggers for "+std::to_string(dmg)+" damage!";
            t.first={-10,-10};
            break;
        }
    }

    void checkQuest() {
        if (quest_.stage==0) {
            for(const auto& x:player_.inv) if(x.kind==ItemKind::Key) {
                quest_.stage=1;
                message_="The Ancient Key hums. Now gather 3 Void Crystals.";
                break;
            }
        }
        if (quest_.stage==1 && quest_.crystals>=3) {
            quest_.stage=2;
            player_.maxMana+=4;
            player_.mana=player_.maxMana;
            message_="The crystals awaken a path to the Warden.";
        }
        if (quest_.stage==2 && player_.floor==3) quest_.complete=true;
    }

    void descend() {
        if (!same(player_.p,stairs_)) { message_="You are not standing on the stairs."; return; }
        if (player_.floor<3) {
            player_.floor++;
            score_+=100*player_.floor;
            generate();
            message_="You descend to floor "+std::to_string(player_.floor)+".";
        } else {
            int wi=-1;
            for(int i=0;i<(int)enemies_.size();i++)
                if(enemies_[i].alive && enemies_[i].kind==EnemyKind::Warden) wi=i;
            if(wi>=0) {
                message_="The Depth Warden guards the final descent. Defeat it.";
            } else {
                message_="The depths are silent. You have won.";
                score_+=1000+player_.level*100;
                render();
                std::cout<<"\nVICTORY. Final score: "<<score_<<"\n";
                running_=false;
            }
        }
    }

    void questScreen() {
        std::cout << "\n=== QUEST ===\n" << questText() << "\n"
                  << "Kills: " << quest_.kills << "\n"
                  << "Crystals: " << quest_.crystals << "\n"
                  << "Stage: " << quest_.stage << "/2\n"
                  << "Press Enter.";
        std::string s; std::getline(std::cin,s);
    }

    void help() {
        std::cout << "\n=== HELP ===\n"
                  << "w/a/s/d move | f melee attack | g pickup\n"
                  << "i inventory | e use/equip | c craft | q quest\n"
                  << "> descend stairs | S save | L load | ? help | x exit\n"
                  << "Enemies act after movement and combat. Explore, loot, level up, and survive.\n"
                  << "Press Enter.";
        std::string s; std::getline(std::cin,s);
    }

    void save() {
        std::ofstream out("savegame.dat");
        if(!out) { message_="Could not open savegame.dat."; return; }
        out << "SD2\n" << seed_ << ' ' << player_.floor << ' ' << player_.turns << ' '
            << player_.p.x << ' ' << player_.p.y << ' ' << player_.hp << ' ' << player_.maxHp << ' '
            << player_.mana << ' ' << player_.maxMana << ' ' << player_.attack << ' ' << player_.defense << ' '
            << player_.level << ' ' << player_.xp << ' ' << player_.nextXp << ' ' << player_.gold << ' '
            << quest_.stage << ' ' << quest_.kills << ' ' << quest_.crystals << ' ' << score_ << '\n';
        for(auto& row:map_) out << row << '\n';
        out << stairs_.x << ' ' << stairs_.y << ' ' << shrine_.x << ' ' << shrine_.y << '\n';
        out << player_.inv.size() << '\n';
        for(const auto& i:player_.inv) out << (int)i.kind << '|' << i.name << '|' << i.power << '|' << i.value << '|' << i.equipped << '\n';
        out << enemies_.size() << '\n';
        for(const auto& e:enemies_) out << e.alive << ' ' << (int)e.kind << ' ' << e.p.x << ' ' << e.p.y << ' '
            << e.hp << ' ' << e.maxHp << ' ' << e.attack << ' ' << e.defense << ' ' << e.xp << ' ' << e.gold << ' ' << e.range << '\n';
        out << ground_.size() << '\n';
        for(const auto& g:ground_) out << g.first.x << ' ' << g.first.y << ' ' << (int)g.second.kind << '|'
            << g.second.name << '|' << g.second.power << '|' << g.second.value << '|' << g.second.equipped << '\n';
        out << traps_.size() << '\n';
        for(const auto& t:traps_) out << t.first.x << ' ' << t.first.y << ' ' << t.second << '\n';
        message_="Game saved.";
    }

    static Item parseItem(const std::string& line) {
        std::stringstream ss(line); std::string part; std::vector<std::string> p;
        while(std::getline(ss,part,'|')) p.push_back(part);
        Item i{ItemKind::Scrap,"Scrap",0,0,false};
        if(p.size()>=5) { i.kind=(ItemKind)std::stoi(p[0]); i.name=p[1]; i.power=std::stoi(p[2]); i.value=std::stoi(p[3]); i.equipped=std::stoi(p[4])!=0; }
        return i;
    }

    void load() {
        std::ifstream in("savegame.dat");
        if(!in) { message_="No savegame.dat found."; return; }
        std::string header; std::getline(in,header);
        if(header!="SD2") { message_="Invalid save file."; return; }
        in >> seed_ >> player_.floor >> player_.turns >> player_.p.x >> player_.p.y
           >> player_.hp >> player_.maxHp >> player_.mana >> player_.maxMana >> player_.attack
           >> player_.defense >> player_.level >> player_.xp >> player_.nextXp >> player_.gold
           >> quest_.stage >> quest_.kills >> quest_.crystals >> score_;
        std::string line; std::getline(in,line);
        for(auto& row:map_) std::getline(in,row);
        in >> stairs_.x >> stairs_.y >> shrine_.x >> shrine_.y;
        size_t n; in >> n; std::getline(in,line);
        player_.inv.clear();
        for(size_t i=0;i<n;i++){std::getline(in,line);player_.inv.push_back(parseItem(line));}
        in >> n; enemies_.clear();
        for(size_t i=0;i<n;i++) {
            Enemy e; int kind;
            in >> e.alive >> kind >> e.p.x >> e.p.y >> e.hp >> e.maxHp >> e.attack >> e.defense >> e.xp >> e.gold >> e.range;
            e.kind=(EnemyKind)kind;
            if(e.kind==EnemyKind::Rat){e.name="Cave Rat";e.glyph='r';}
            else if(e.kind==EnemyKind::Goblin){e.name="Goblin Raider";e.glyph='g';}
            else if(e.kind==EnemyKind::Archer){e.name="Ash Archer";e.glyph='a';}
            else if(e.kind==EnemyKind::Brute){e.name="Stone Brute";e.glyph='B';}
            else {e.name="Depth Warden";e.glyph='W';}
            enemies_.push_back(e);
        }
        in >> n; std::getline(in,line); ground_.clear();
        for(size_t i=0;i<n;i++) {
            int x,y; in >> x >> y; std::getline(in,line);
            ground_.push_back({{x,y},parseItem(line.substr(1))});
        }
        in >> n; traps_.clear();
        for(size_t i=0;i<n;i++){int x,y,d;in>>x>>y>>d;traps_.push_back({{x,y},d});}
        reveal();
        message_="Game loaded.";
    }
};

} // namespace

int main(int argc,char** argv) {
    uint64_t seed;
    if(argc>1) {
        try { seed=std::stoull(argv[1]); }
        catch(...) { seed=static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()); }
    } else {
        seed=static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }
    std::cout << "Starting The Shattered Depths with seed " << seed << "...\n";
    Game game(seed);
    game.run();
    return 0;
}
