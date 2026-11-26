// agentic_weather_map.cpp — heuristic, agent-driven weather-field simulation (C++20)
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
constexpr int W=72,H=20; constexpr bool Unicode=true;
constexpr std::string_view SUN=Unicode?"☀":"O", CLOUD=Unicode?"☁":"o", STORM=Unicode?"☈":"X";
volatile std::sig_atomic_t stop_flag=0;
struct RGB { std::uint8_t r{},g{},b{}; };
struct V { float x{},y{}; V& operator+=(V b){x+=b.x;y+=b.y;return *this;} friend V operator+(V a,V b){return {a.x+b.x,a.y+b.y};} friend V operator-(V a,V b){return {a.x-b.x,a.y-b.y};} friend V operator*(V a,float s){return {a.x*s,a.y*s};} };
float len(V v){return std::sqrt(v.x*v.x+v.y*v.y);} float clamp(float v,float lo,float hi){return std::clamp(v,lo,hi);} float wrap(float v,float m){v=std::fmod(v,m);return v<0?v+m:v;}
RGB lerp(RGB a,RGB b,float t){auto m=[t](int x,int y){return static_cast<std::uint8_t>(std::lround(x+(y-x)*t));};return {m(a.r,b.r),m(a.g,b.g),m(a.b,b.b)};}
struct Cell{float temp=15,humidity=.35f;};
struct Field{std::array<Cell,W*H> cells{};Cell& at(int x,int y){return cells[std::size_t(y)*W+x];}const Cell& at(int x,int y)const{return cells[std::size_t(y)*W+x];} Cell sample(V p)const{return at(std::clamp(int(p.x),0,W-1),std::clamp(int(p.y),0,H-1));}static bool inside(int x,int y){return x>=0&&y>=0&&x<W&&y<H;}};
V gradient(const Field& f,V p){int x=std::clamp(int(p.x),0,W-1),y=std::clamp(int(p.y),0,H-1);auto h=[&](int xx,int yy){return f.at(std::clamp(xx,0,W-1),std::clamp(yy,0,H-1)).humidity;};return {(h(x+1,y)-h(x-1,y))*.5f,(h(x,y+1)-h(x,y-1))*.5f};}
struct Agent{V pos{},vel{};int age=0,ttl=0;};struct Sun:Agent{};struct Cloud:Agent{};struct Storm:Agent{};using Any=std::variant<Sun,Cloud,Storm>;
template<class T>concept WeatherAgent=std::derived_from<T,Agent>;static_assert(WeatherAgent<Sun>&&WeatherAgent<Cloud>&&WeatherAgent<Storm>);
class Map{
public: explicit Map(std::uint32_t seed):rng(seed){seed_field();for(int i=0;i<5;++i)sun();for(int i=0;i<6;++i)cloud();for(int i=0;i<2;++i)storm();}
void step(){++frame;time+=1;for(auto& a:agents)std::visit([this](auto& x){update(x);},a);diffuse();relax();lifecycle();}
std::string render()const;
private:
static float baseline(float fx,float fy){return 26-34*fy+4*std::sin(fx*6.2831853f*1.7f+.6f)+3*std::cos(fy*6.2831853f*2.3f);}
void seed_field(){for(int y=0;y<H;++y)for(int x=0;x<W;++x){float fx=float(x)/W,fy=float(y)/H;float h=.35f+.22f*std::sin(fx*6.2831853f*2.1f+1.1f)*std::cos(fy*6.2831853f*1.4f)+.15f*std::sin((fx+fy)*6.2831853f*3.1f);field.at(x,y)={baseline(fx,fy),clamp(h,.02f,.98f)};}}
V wind(V p)const{float t=time*.05f;return {.30f+.22f*std::sin(p.y*.33f+t),.05f*std::cos(p.x*.21f+t*.7f)};}
template<class F>void affect(V p,float radius,F&& fn){for(int y=std::max(0,int(p.y-radius));y<=std::min(H-1,int(p.y+radius));++y)for(int x=std::max(0,int(p.x-radius));x<=std::min(W-1,int(p.x+radius));++x){float dx=x+.5f-p.x,dy=y+.5f-p.y,d=std::sqrt(dx*dx+dy*dy);if(d<=radius)fn(field.at(x,y),1-d/radius);}}
void steer(Agent&a,V target,float maxspeed){V d=target-a.vel;float n=len(d);if(n>.25f)d=d*(.25f/n);a.vel+=d;float sp=len(a.vel);if(sp>maxspeed)a.vel=a.vel*(maxspeed/sp);a.pos+=a.vel;a.pos.x=wrap(a.pos.x,float(W));a.pos.y=clamp(a.pos.y,1,float(H)-1.5f);}
void update(Sun&s){auto g=gradient(field,s.pos);steer(s,wind(s.pos)*.4f-g*6,.55f);affect(s.pos,1.8f,[](Cell&c,float w){c.temp+=1.10f*w;c.humidity-=.020f*w;});++s.age;}
void update(Cloud&c){auto g=gradient(field,c.pos);steer(c,wind(c.pos)+g*3,.80f);affect(c.pos,2.1f,[](Cell&v,float w){v.humidity+=.050f*w;v.temp-=.250f*w;});++c.age;}
void update(Storm&s){auto g=gradient(field,s.pos);V swirl{-g.y,g.x};steer(s,wind(s.pos)*1.6f+swirl*4+g*1.5f,1.4f);affect(s.pos,2.6f,[](Cell&c,float w){c.humidity-=.060f*w;c.temp-=.500f*w;});++s.age;}
void diffuse(){auto next=field.cells;constexpr float k=.08f;for(int y=0;y<H;++y)for(int x=0;x<W;++x){int l=std::max(0,x-1),r=std::min(W-1,x+1),u=std::max(0,y-1),d=std::min(H-1,y+1);float nt=.25f*(field.at(l,y).temp+field.at(r,y).temp+field.at(x,u).temp+field.at(x,d).temp),nh=.25f*(field.at(l,y).humidity+field.at(r,y).humidity+field.at(x,u).humidity+field.at(x,d).humidity);Cell&c=next[std::size_t(y)*W+x];c.temp+=(nt-c.temp)*k;c.humidity+=(nh-c.humidity)*k;}field.cells=next;}
void relax(){for(int y=0;y<H;++y)for(int x=0;x<W;++x){Cell&c=field.at(x,y);float fx=float(x)/W,fy=float(y)/H;c.temp+=(baseline(fx,fy)-c.temp)*.010f;c.humidity+=(.35f-c.humidity)*.004f;c.temp=clamp(c.temp,-25,45);c.humidity=clamp(c.humidity,0,1);}}
float random01(){return std::uniform_real_distribution<float>(0,1)(rng);}V random_pos(){return {random01()*W,2+random01()*(H-4)};}void sun(){Sun a;a.pos=random_pos();a.ttl=900+int(random01()*600);agents.emplace_back(a);}void cloud(){Cloud a;a.pos=random_pos();a.ttl=400+int(random01()*500);agents.emplace_back(a);}void storm(){Storm a;a.pos=random_pos();a.ttl=90+int(random01()*140);agents.emplace_back(a);}
template<WeatherAgent T>int count()const{return int(std::count_if(agents.begin(),agents.end(),[](const Any&a){return std::holds_alternative<T>(a);}));}
void lifecycle(){std::erase_if(agents,[](const Any&a){return std::visit([](const auto&v){return v.age>=v.ttl;},a);});for(auto&a:agents)if(auto*c=std::get_if<Cloud>(&a);c&&field.sample(c->pos).humidity>.82f&&random01()<.03f){Storm s;s.pos=c->pos;s.vel=c->vel*1.4f;s.ttl=100+int(random01()*160);a=s;break;}if(count<Sun>()<6&&random01()<.05f)sun();if(count<Cloud>()<7&&random01()<.10f)cloud();}
static RGB temp_color(float t){struct Stop{float t;RGB c;};static constexpr Stop stops[]={{-25,{30,40,120}},{-10,{50,110,200}},{0,{90,190,230}},{10,{110,215,160}},{18,{235,220,95}},{26,{245,165,60}},{34,{230,85,50}},{45,{150,25,60}}};if(t<=stops[0].t)return stops[0].c;for(std::size_t i=1;i<std::size(stops);++i)if(t<=stops[i].t){float f=(t-stops[i-1].t)/(stops[i].t-stops[i-1].t);return lerp(stops[i-1].c,stops[i].c,f);}return stops[std::size(stops)-1].c;}
static std::string_view precip(float h){if(h<.30f)return " ";if(h<.45f)return ".";if(h<.60f)return ":";if(h<.72f)return "*";if(h<.85f)return "%";return "#";}
static void bg(std::string&s,RGB c){char b[24];std::snprintf(b,sizeof b,"\x1b[48;2;%d;%d;%dm",int(c.r),int(c.g),int(c.b));s+=b;}static void fg(std::string&s,RGB c){char b[24];std::snprintf(b,sizeof b,"\x1b[38;2;%d;%d;%dm",int(c.r),int(c.g),int(c.b));s+=b;}
Field field{};std::vector<Any>agents;std::mt19937 rng;float time=0;int frame=0;
};
std::string Map::render()const{std::size_t n=W*H;std::vector<std::string_view> glyph(n," ");std::vector<RGB>fgs(n),bgs(n);for(int y=0;y<H;++y)for(int x=0;x<W;++x){auto i=std::size_t(y)*W+x;const Cell&c=field.at(x,y);bgs[i]=temp_color(c.temp);fgs[i]={22,24,30};glyph[i]=precip(c.humidity);}for(const auto&v:agents)std::visit([&](const auto&a){using T=std::decay_t<decltype(a)>;int x=int(a.pos.x),y=int(a.pos.y);if(!Field::inside(x,y))return;auto i=std::size_t(y)*W+x;if constexpr(std::is_same_v<T,Sun>){glyph[i]=SUN;fgs[i]={70,45,0};bgs[i]={255,205,60};}else if constexpr(std::is_same_v<T,Cloud>){glyph[i]=CLOUD;fgs[i]={245,248,255};bgs[i]={110,118,135};}else{glyph[i]=STORM;fgs[i]={255,255,255};bgs[i]={140,40,200};}},v);std::string out;out.reserve(n*20+1024);char head[192];std::snprintf(head,sizeof head,"\x1b[1m AGENTIC WEATHER MAP \x1b[22m  step %5d   sun %d  cloud %d  storm %d\x1b[K\n",frame,count<Sun>(),count<Cloud>(),count<Storm>());out+=head;bool hf=false,hb=false;RGB cf{},cb{};for(int y=0;y<H;++y){for(int x=0;x<W;++x){auto i=std::size_t(y)*W+x;if(!hb||bgs[i].r!=cb.r||bgs[i].g!=cb.g||bgs[i].b!=cb.b){bg(out,bgs[i]);cb=bgs[i];hb=true;}if(!hf||fgs[i].r!=cf.r||fgs[i].g!=cf.g||fgs[i].b!=cf.b){fg(out,fgs[i]);cf=fgs[i];hf=true;}out+=glyph[i];}out+="\x1b[0m\x1b[K\n";hf=hb=false;}out+='\n';for(int t=-20;t<=40;t+=5){bg(out,temp_color(float(t)));out+="   ";}out+="\x1b[0m\x1b[K\n ";for(int t=-20;t<=40;t+=5){char label[8];std::snprintf(label,sizeof label,"%3d",t);out+=label;}out+="  \xC2\xB0" "C\x1b[K\n";out+=" rain: . damp  : wet  * heavy  # downpour    ";out+=SUN;out+=" sun   ";out+=CLOUD;out+=" cloud   ";out+=STORM;out+=" storm\x1b[K\n";return out;}
class TerminalGuard{public:TerminalGuard(){std::cout<<"\x1b[?1049h\x1b[?25l\x1b[2J";}~TerminalGuard(){std::cout<<"\x1b[0m\x1b[?25h\x1b[?1049l"<<std::flush;}};
void on_signal(int){stop_flag=1;}
}
int main(int argc,char**argv){int steps=2000,delay=70;std::uint32_t seed=std::random_device{}();for(int i=1;i<argc;++i){std::string_view a=argv[i];auto parse=[&](int&v){if(i+1>=argc)return false;char*end=nullptr;long n=std::strtol(argv[++i],&end,10);if(!end||*end!='\0'||n<0||n>10000000)return false;v=int(n);return true;};if(a=="--steps"){if(!parse(steps)){std::cerr<<"invalid --steps value\n";return 2;}}else if(a=="--delay"){if(!parse(delay)||delay>60000){std::cerr<<"invalid --delay value (0..60000 ms)\n";return 2;}}else if(a=="--seed"){int s;if(!parse(s)){std::cerr<<"invalid --seed value\n";return 2;}seed=std::uint32_t(s);}else if(a=="--help"){std::cout<<"usage: weather [--steps N] [--delay MS] [--seed N]\n";return 0;}else{std::cerr<<"usage: weather [--steps N] [--delay MS] [--seed N]\n";return 2;}}std::signal(SIGINT,on_signal);std::ios_base::sync_with_stdio(false);TerminalGuard guard;Map map(seed);for(int i=0;i<steps&&!stop_flag;++i){map.step();std::cout<<"\x1b[H"<<map.render()<<std::flush;if(delay)std::this_thread::sleep_for(std::chrono::milliseconds(delay));}return 0;}
