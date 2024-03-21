#include <algorithm>
#include <apsi/log.h>
#include <apsi/oprf/ecpoint.h>
#include "../circuit/TripleGen.h"
#include "../shuffle/ShareCorrelationGen.h"
#include "MPSU.h"
#include "MPSI.h"
#include <csignal>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

std::uint64_t hexStringToDecimal64(const std::string& hexValue) {
    try {
        std::uint64_t decimalValue = std::stoull(hexValue, nullptr, 16);
        return decimalValue;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument exception: " << e.what() << std::endl;
    } catch (const std::out_of_range& e) {
        std::cerr << "Out of range exception: " << e.what() << std::endl;
    }
    // 返回默认值或者抛出异常，根据您的需求
    return 0; // 或者抛出异常
}

std::vector<block> readDataFromCSV(const std::string& filename, u32 numElements, u32 idx) {
    std::vector<block> set(numElements);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        // 文件打开失败，处理错误情况
        return set;
        
    }
    std::string line;
    u32 i = 0;
    //先读一行，舍弃表头
    std::getline(file, line);
    while (std::getline(file, line) && i < numElements) {
        std::istringstream iss(line);
        std::string hexValue;
        if (!(iss >> hexValue)) {
            std::cout<<"无法解析行中的数据，处理错误情况:";
            continue;
        }
        // std::cout<< idx <<":"<< hexValue << std::endl;
        // 将十六进制字符串转换为 64 位整数
        uint64_t intValue = hexStringToDecimal64(hexValue);
        // 将整数转换为 block 类型并存储到 set 中
        set[i++] = oc::toBlock(intValue);
    }

    file.close();
    return set;
}

std::vector<block> MPSU_test_front(u32 idx, u32 numElements, u32 numParties, u32 numThreads, bool fakeBase, bool fakeTriples){
    ShareCorrelation sc(numParties, (numParties - 1) * numElements);
    if (!sc.exist()){
        return std::vector<block>();
    }
    std::vector<block> set(numElements);

    // generate set
    // for (u32 i = 0; i < numElements; i++){
    //     set[i] = oc::toBlock(idx*100 + i);
    //     // std::cout << set[i] << std::endl;
    // }

    //从csv里面读取id，然后转化位block版本的set数组
    std::string filename = "./data_id/data_"+std::to_string(idx)+".csv";  // CSV 文件名
    // 从 CSV 文件中读取数据并转换为 block 类型
    set = readDataFromCSV(filename, numElements, idx);

    // for (u32 i = 0; i < set.size(); i++){
    //     long long low_bits = set[i].mData[0]; // 假设 mData 是 block 结构的成员变量
    //         // 转换为 u64 类型
    //     u64 value = static_cast<u64>(low_bits);
    //     std::cout << value<< std::endl;
    // } 
    return set;
}
std::string uint64_to_hex_string(std::uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << (value & 0xFFFFFFFF);
    return oss.str();
}
int main(int agrc, char** argv){
    u32 n =  1ull << 2;//n：每个集合中的元素数量，默认为 1024。
    u32 k = 3;//参与方的数量，默认为 3。
    u32 nt = 1;//nt：线程数量，默认为 1。
    bool fakeBase = 1 ;//!(cmd.isSet("genBase"));生成基础 OTs（Oblivious Transfers），是指生成用于保护隐私的基础数据。
    bool fakeTriples = 0 ;//cmd.isSet("fakeTriple"); fakeTriple：使用虚假布尔三元组 
    std::vector<block> set(n);
    std::vector<u64> result_psu;
    std::vector<std::string> hex_strings;
    // const char *home_dir = std::getenv("HOME");
    // std::string output_dir = home_dir;
    // output_dir += "/Desktop/APSI/build/";
    // std::string output_path = output_dir + "output.csv";
    // std::cout<<output_path <<std::endl;
    


    CLP cmd;
    cmd.parse(agrc, argv);
    u32 idx = cmd.getOr("r", -1);

    //程序开始
    //std::cout << "program begin" << std::endl;

    //开始生成个什么玩意
    //std::cout << "generate sc begin" << std::endl;
    ShareCorrelation sc(k, (k - 1) * n);
    sc.generate();
    sc.writeToFile();
    sc.release();
    //std::cout << "generate sc done" << std::endl;

    ///又生成个什么玩意
    tripleGenParty(idx, k, n, nt);

    //开始MPSU_test
    set = MPSU_test_front(idx, n, k, nt, fakeBase, fakeTriples);


    if (idx == 0){
        std::vector<block> out;
        out = MPSUParty(idx, k, n, set, nt, fakeBase, fakeTriples);
        std::ofstream csv_file("/home/malanbo/Desktop/APSI/build/out.csv");
        for (u32 i = 0; i < out.size(); i++){
            long long low_bits = out[i].mData[0]; // 假设 mData 是 block 结构的成员变量
            // 转换为 u64 类型
            u64 value = static_cast<u64>(low_bits);
            //result_psu.push_back(value);
            std::string hex_str = uint64_to_hex_string(value);
            hex_strings.push_back(hex_str);
            csv_file << hex_str << std::endl;
        } 
    } else {
        MPSUParty(idx, k, n, set, nt, fakeBase, fakeTriples);
    }
    //std::cout << "psu program END" << std::endl;
    //std::cout << "psi program begin" << std::endl;



////sender
   // std::shared_ptr<apsi::sender::SenderDB> sender_db;
    //apsi::oprf::OPRFKey oprf_key;

}