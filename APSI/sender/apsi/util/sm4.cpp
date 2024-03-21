#include "sm4.h"
void SM4::set_key(uint64_t key_high, uint64_t key_low) {
    uint32_t MK[5]={0};
    int t = 0;
    MK[3] = static_cast<uint32_t>(key_low & 0xFFFFFFFF)^ FK[3];
    MK[2] = static_cast<uint32_t>((key_low >> 32) & 0xFFFFFFFF)^ FK[2];
    MK[1] = static_cast<uint32_t>(key_high & 0xFFFFFFFF)^ FK[1];
    MK[0] = static_cast<uint32_t>((key_high >> 32) & 0xFFFFFFFF)^ FK[0];
    for (int i = 0; i < 32; i++)
    {
        uint32_t box_in = MK[1] ^ MK[2] ^ MK[3] ^ CK_32[i];
        uint32_t box_out = T(box_in);
        rk[i] = MK[0] ^ box_out;
        MK[4] = rk[i];
        for (t = 0; t < 4; t++){
            MK[t] = MK[t + 1];
        }
    }
}

uint32_t SM4::T(uint32_t in) {
    unsigned int temp_A = in;
    unsigned int temp_B;
    unsigned int temp_C;
    //Sbox非线性变换
    temp_B = (S_Box[(temp_A >> 24) & 0xFF] << 24) | (S_Box[(temp_A >> 16) & 0xFF] << 16)
             | (S_Box[(temp_A >> 8) & 0xFF] << 8) | S_Box[temp_A & 0xFF];
    //非线性变换
    temp_C = temp_B ^ left_shift_loop(temp_B, 13) ^ left_shift_loop(temp_B, 23);
    return temp_C;
}

int SM4::left_shift_loop(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

pair<uint64_t, uint64_t> SM4::encrypt(uint64_t block_high, uint64_t block_low) {
    uint32_t X[64];
    X[3] = static_cast<uint32_t>(block_low & 0xFFFFFFFF);
    X[2] = static_cast<uint32_t>((block_low >> 32) & 0xFFFFFFFF);
    X[1] = static_cast<uint32_t>(block_high & 0xFFFFFFFF);
    X[0] = static_cast<uint32_t>((block_high >> 32) & 0xFFFFFFFF);
    for (int i = 0; i < 32; i++)
    {
        uint32_t box_in = X[1] ^ X[2] ^ X[3] ^ rk[i];
        uint32_t box_out = s_turn(box_in);
        X[4] = X[0] ^ box_out;
        for (int t = 0; t < 4; t++)
            X[t] = X[t + 1];
    }
    pair<uint64_t, uint64_t> result = reverse_sm4(X[3], X[2], X[1], X[0]);
    return result;
}

uint32_t SM4::s_turn(uint32_t in) {
    unsigned int temp_A = in;
    unsigned int temp_B;
    unsigned int temp_C;
    //Sbox非线性变换
    temp_B = (S_Box[(temp_A >> 24) & 0xFF] << 24) | (S_Box[(temp_A >> 16) & 0xFF] << 16)
             | (S_Box[(temp_A >> 8) & 0xFF] << 8) | S_Box[temp_A & 0xFF];
    temp_C = temp_B ^ left_shift_loop(temp_B, 2) ^ left_shift_loop(temp_B, 10)^ left_shift_loop(temp_B, 18)^ left_shift_loop(temp_B, 24);
    return temp_C;
}

std::pair<uint64_t, uint64_t> SM4::reverse_sm4(uint32_t i, uint32_t i1, uint32_t i2, uint32_t i3) {
    uint64_t part1 = static_cast<uint64_t>(i) << 32 | i1;
    uint64_t part2 = static_cast<uint64_t>(i2) << 32 | i3;
    return std::make_pair(part1, part2);
}

SM4::SM4() = default;