#pragma once
#include "render/IrradianceVolume.hpp"

namespace azurerender {
// Static directional clipmap data. Coordinates and coverage are authored in
// field space; the rigid world transform may include a reflected axis.
class IrradianceClipmap {
public:
    using Row=std::array<float,4>;
    struct Layer {Row centerSpacing{0,0,0,1},fade{1,1,1,0};};
    static constexpr std::size_t headerBytes=224,maxVoxels=1U<<22;
    std::array<std::uint32_t,3> dimensions{1,1,1};
    bool enabled=true;
    std::array<Row,3> worldToField{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    std::array<Row,3> defaultCoefficients{};
    std::array<Layer,3> layers{};
    std::array<std::vector<std::uint32_t>,3> amplitude;
    std::array<std::vector<std::uint8_t>,3> directional;

    void validate() const {
        validateShape();
        for(const auto& layer:amplitude)for(auto packed:layer)(void)unpack(packed);
    }
    [[nodiscard]] std::array<Row,13> shaderParameters() const {
        validateShape();std::array<Row,13> rows{};
        rows[0]={float(dimensions[0]),float(dimensions[1]),float(dimensions[2]),enabled?1.F:0.F};
        for(unsigned i=0;i<3;++i){rows[1+i]=worldToField[i];rows[4+i]=defaultCoefficients[i];
            rows[7+2*i]=layers[i].centerSpacing;rows[8+2*i]=layers[i].fade;}
        return rows;
    }
    [[nodiscard]] IrradianceSample query(const std::array<float,3>& world) const {
        validateShape();std::array<float,3> p{};
        for(unsigned a=0;a<3;++a){
            if(!std::isfinite(world[a]))throw std::invalid_argument("Nonfinite clipmap position");
        }
        for(unsigned a=0;a<3;++a){p[a]=worldToField[a][3];
            for(unsigned b=0;b<3;++b)p[a]+=worldToField[a][b]*world[b];
            if(!std::isfinite(p[a]))throw std::invalid_argument("Clipmap transformed position overflow");}
        std::array<float,3> fades{};
        const auto saturate=[](float f){return std::clamp(f,0.F,1.F);};
        for(unsigned i=0;i<3;++i){const auto& l=layers[i];
            const auto x=std::abs(p[0]-l.centerSpacing[0]),y=std::abs(p[1]-l.centerSpacing[1]),z=std::abs(p[2]-l.centerSpacing[2]);
            fades[i]=std::max(saturate((std::max(x,z)-l.fade[0])/l.fade[2]),saturate((y-l.fade[1])/l.fade[2]));}
        auto coefficients=defaultCoefficients;
        if(enabled&&fades[2]<1){
            const std::array<float,3> weights={1-fades[0],fades[0]*(1-fades[1]),fades[1]*(1-fades[2])};
            std::array<std::array<float,3>,3> amplitudes{};std::array<std::array<Row,3>,3> directions{};
            float alpha=fades[2];
            for(unsigned i=0;i<3;++i){if(weights[i]==0)continue;
                sample(i,p,amplitudes[i],directions[i]);alpha+=weights[i]*directions[i][0][3];}
            const auto remapped=saturate(2*alpha-1),u=remapped-fades[2],v=.5F*(remapped+fades[2]);
            for(unsigned c=0;c<3;++c){
                coefficients[c]={0,0,0,0};
                for(unsigned i=0;i<3;++i){const float magnitude=weights[i]*amplitudes[i][c];
                    for(unsigned a=0;a<3;++a)coefficients[c][a]+=magnitude*(4*directions[i][c][a]-2);
                    coefficients[c][3]+=magnitude;}
                for(unsigned a=0;a<4;++a)coefficients[c][a]+=v*defaultCoefficients[c][a];
                coefficients[c][1]+=.5F*u*defaultCoefficients[c][3];
                coefficients[c][3]+=.375F*u*defaultCoefficients[c][1];
            }
        }
        IrradianceSample result;result.weight=1;
        for(unsigned c=0;c<3;++c){result.coefficients[c][3]=coefficients[c][3];
            for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b)
                result.coefficients[c][a]+=worldToField[b][a]*coefficients[c][b];}
        return result;
    }
    [[nodiscard]] std::vector<std::byte> encode() const {
        validate();std::vector<std::byte> bytes;bytes.reserve(headerBytes+voxelCount()*48);
        for(char c:std::array<char,8>{'A','Z','I','C','0','0','0','1'})bytes.push_back(std::byte(c));
        auto put=[&](std::uint32_t value){for(unsigned shift=0;shift<32;shift+=8)bytes.push_back(std::byte((value>>shift)&255));};
        auto row=[&](const Row& r){for(float f:r){std::uint32_t bits;std::memcpy(&bits,&f,4);put(bits);}};
        for(auto d:dimensions)put(d);put(enabled?1U:0U);
        for(const auto& r:worldToField)row(r);for(const auto& r:defaultCoefficients)row(r);
        for(const auto& l:layers){row(l.centerSpacing);row(l.fade);}put(0);put(0);
        for(unsigned i=0;i<3;++i){for(auto packed:amplitude[i])put(packed);
            for(auto channel:directional[i])bytes.push_back(std::byte(channel));}
        return bytes;
    }
    [[nodiscard]] static IrradianceClipmap decode(const std::vector<std::byte>& bytes){
        if(bytes.size()<headerBytes||std::memcmp(bytes.data(),"AZIC0001",8)!=0)
            throw std::invalid_argument("Invalid irradiance clipmap version/header");
        std::size_t offset=8;
        auto get=[&](){std::uint32_t value=0;for(unsigned s=0;s<32;s+=8)value|=std::uint32_t(bytes[offset++])<<s;return value;};
        auto row=[&](Row& r){for(auto& f:r){auto bits=get();std::memcpy(&f,&bits,4);}};
        IrradianceClipmap v;for(auto& d:v.dimensions)d=get();const auto flags=get();
        if(flags>1)throw std::invalid_argument("Invalid clipmap activation flags");v.enabled=flags!=0;
        for(auto& r:v.worldToField)row(r);for(auto& r:v.defaultCoefficients)row(r);
        for(auto& l:v.layers){row(l.centerSpacing);row(l.fade);}
        if(get()!=0||get()!=0)throw std::invalid_argument("Invalid clipmap reserved fields");
        const auto count=v.voxelCount();
        if(bytes.size()!=headerBytes+48*count)throw std::invalid_argument("Clipmap payload size differs");
        for(unsigned i=0;i<3;++i){v.amplitude[i].resize(count);for(auto& p:v.amplitude[i])p=get();
            v.directional[i].resize(count*12);for(auto& p:v.directional[i])p=std::uint8_t(bytes[offset++]);}
        v.validate();return v;
    }
    [[nodiscard]] static IrradianceClipmap load(const std::filesystem::path& path){
        std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::invalid_argument("Cannot open irradiance clipmap");
        const auto size=f.tellg();if(size<0||std::uint64_t(size)>headerBytes+48*maxVoxels)
            throw std::invalid_argument("Irradiance clipmap exceeds budget");
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));f.seekg(0);
        if(!f.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))throw std::invalid_argument("Cannot read irradiance clipmap");
        return decode(bytes);
    }
private:
    [[nodiscard]] std::size_t voxelCount() const {
        std::size_t count=1;for(auto d:dimensions){if(d==0||d>maxVoxels/count)
            throw std::invalid_argument("Clipmap dimensions exceed budget");count*=d;}return count;
    }
    void validateShape() const {
        const auto count=voxelCount();
        for(const auto& r:worldToField)for(auto f:r)if(!std::isfinite(f))throw std::invalid_argument("Nonfinite clipmap transform");
        for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){float dot=0;
            for(unsigned c=0;c<3;++c)dot+=worldToField[a][c]*worldToField[b][c];
            if(std::abs(dot-(a==b?1.F:0.F))>1e-5F)throw std::invalid_argument("Clipmap transform requires orthonormal axes");}
        for(const auto& r:defaultCoefficients)for(auto f:r)if(!std::isfinite(f))throw std::invalid_argument("Nonfinite clipmap default SH");
        for(unsigned i=0;i<3;++i){const auto& l=layers[i];
            for(const auto& r:{l.centerSpacing,l.fade})for(auto f:r)if(!std::isfinite(f))throw std::invalid_argument("Nonfinite clipmap layer");
            if(l.centerSpacing[3]<=0||l.fade[0]<0||l.fade[1]<0||l.fade[2]<=0||l.fade[3]!=0)
                throw std::invalid_argument("Invalid clipmap spacing/coverage");
            if(amplitude[i].size()!=count||directional[i].size()!=count*12)throw std::invalid_argument("Invalid clipmap texture length");
        }
    }
    [[nodiscard]] static std::array<float,3> unpack(std::uint32_t packed){
        std::array<float,3> value{};const unsigned widths[3]={6,6,5},shifts[3]={0,11,22};
        for(unsigned c=0;c<3;++c){const auto m=widths[c],bits=packed>>shifts[c],exponent=(bits>>m)&31U,mantissa=bits&((1U<<m)-1);
            if(exponent==31)throw std::invalid_argument("Nonfinite clipmap amplitude");
            value[c]=exponent?std::ldexp(1.F+float(mantissa)/float(1U<<m),int(exponent)-15):std::ldexp(float(mantissa),-14-int(m));}
        return value;
    }
    void sample(unsigned layer,const std::array<float,3>& p,std::array<float,3>& rgb,std::array<Row,3>& directions) const {
        std::array<double,3> uv{};
        for(unsigned a=0;a<3;++a){const auto q=(double(p[a])/layers[layer].centerSpacing[3]+.5)/dimensions[a];uv[a]=q-std::floor(q);}
        const auto interpolate=[&](std::array<double,3> coordinate,const std::array<unsigned,3>& size,bool wrap){
            std::array<int,3> base{};std::array<double,3> fraction{};
            for(unsigned a=0;a<3;++a){const auto texel=coordinate[a]*size[a]-.5;base[a]=int(std::floor(texel));fraction[a]=texel-base[a];}
            Row result{};
            for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){
                const int corner[3]={x,y,z};std::array<unsigned,3> at{};double w=1;
                for(unsigned a=0;a<3;++a){const int n=int(size[a]),index=base[a]+corner[a];
                    at[a]=unsigned(wrap?(index%n+n)%n:std::clamp(index,0,n-1));w*=corner[a]?fraction[a]:1-fraction[a];}
                const auto index=at[0]+std::size_t(size[0])*(at[1]+std::size_t(size[1])*at[2]);
                if(wrap){const auto v=unpack(amplitude[layer][index]);for(unsigned c=0;c<3;++c)result[c]+=float(w*v[c]);}
                else for(unsigned c=0;c<4;++c)result[c]+=float(w*directional[layer][index*4+c]/255.);
            }
            return result;
        };
        const auto a=interpolate(uv,dimensions,true);for(unsigned c=0;c<3;++c)rgb[c]=a[c];
        uv[1]=std::clamp(uv[1],.5/dimensions[1],1-.5/dimensions[1])/3;
        auto size=dimensions;size[1]*=3;
        for(unsigned c=0;c<3;++c){auto q=uv;q[1]+=double(c)/3;directions[c]=interpolate(q,size,false);}
    }
};
}
