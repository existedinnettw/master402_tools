#include <pugixml.hpp>
#include <iostream>
#include <ecrt.h>
#include <argparse/argparse.hpp>
#include <string>
#include <boost/process.hpp>
// #include <format>
#include <vector>
using namespace boost::process;

uint32_t hex_or_dec_str_to_int(std::string int_str)
{
    if (int_str.rfind("#x", 0) == 0)
    {
        // start with #x
        // 16bit case
        int_str.erase(std::remove(int_str.begin(), int_str.end(), '#'), int_str.end());
        int_str.erase(std::remove(int_str.begin(), int_str.end(), 'x'), int_str.end());
        // int_str.erase(0, 2);
        // std::cout<<int_str<<"\n";
        return std::stoi(int_str, 0, 16);
    }
    else
    {
        return std::stoi(int_str, 0, 10);
    }
}

void execute_cmd(std::string s)
{
    std::cout << "cmd: " << s << "\n";
    ipstream pipe_stream;
    std::string line;
    // s="gcc --version";
    child c(s, std_out > pipe_stream);
    while (pipe_stream && std::getline(pipe_stream, line) && !line.empty())
        std::cerr << line << std::endl;
    c.wait();
    printf("\n");
};

std::string join_cmd(const std::vector<std::string> &v, char c)
{
    std::string s;

    for (std::vector<std::string>::const_iterator p = v.begin();
         p != v.end(); ++p)
    {
        s += *p;
        if (p != v.end() - 1)
            s += c;
    }

    // clear additional space
    auto isBothSpace = [](char const &lhs, char const &rhs) -> bool
    {
        return lhs == rhs && iswspace(lhs);
    };
    auto it = std::unique(s.begin(), s.end(), isBothSpace);
    s.erase(it, s.end());

    return s;
}
std::string int2hex_string(uint32_t var)
{
    std::stringstream ss;
    ss << "0x" << std::hex << var;
    return ss.str();
}
auto cv = [](std::string in)
{
    return int2hex_string(hex_or_dec_str_to_int(in));
};

class Option
{
public:
    Option(std::string flag, std::string value) : flag(flag), value(value){};
    std::string render()
    {
        if (!strcmp(value.c_str(), ""))
        {
            return "";
        }
        else
        {
            return flag + " " + value;
        }
    }

private:
    std::string flag;
    std::string value;
};

/**
 * seems better map without add subindex 1
 * @see https://en.nanotec.com/products/manual/PD6C_CANopen_USB_EN/object_dictionary%2Fod_motion_0x1600.html?cHash=6b4ddca6f595e14538f233c2ee72ad5e
 * @todo based on return value `./your_program; echo $?` check if success or not;
*/
int main(int argc, char *argv[])
{
    /**
     * arg parse part
     * @see [argparse](https://github.com/p-ranav/argparse)
     */
    argparse::ArgumentParser program("pdo mapping", "0.0.1");
    program.add_description(
        "A small tool to map pdo through igh master and esi file.\n"
        "Some SDO Abort Code may help when `ethercat download` occure.\n"
        "0x06010003: Entry can not be written because Subindex0 is not 0.\n"
    );
    program.add_argument("input_file_path").help("path to read esi file e.g. ~/esi/HDT_TomCatEvo_Ethercat_V4.xml");
    program.add_argument("-a", "--alias").help("alias of slave.").default_value("");              // with default value, eazier to handle.
    program.add_argument("-p", "--position").help("position of slave on bus.").default_value(""); //.required()
    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }
    std::string input_file_path = program.get<std::string>("input_file_path");
    std::cout << "your input path: " << program.get<std::string>("input_file_path") << "\n";

    /**
     * xml part
     */
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(input_file_path.c_str());
    if (!result)
        return -1;
    std::cout << "reading success.\n";
    /// EtherCATInfo/Descriptions/Devices
    std::cout << "name: " << doc.child("EtherCATInfo").child("Descriptions").child("Devices").child("Device").child("Name").text().get() << std::endl;
    std::cout << "\n";
    // std::cout <<"name: " << doc.child("EtherCATInfo").child("Descriptions").child("Devices").child("Device").child("Name").value() << std::endl;
    // throw;

    /**
     * 找到rxpdos 下的所有entry, 一一sdo config。
     * @see [5.8. Working with text contents](https://pugixml.org/docs/manual.html#access.text)
     * @see [cia402 Process data object (PDO)](https://www.can-cia.org/can-knowledge/canopen/pdo-protocol)
     */
    auto ex_map = [&](std::string pdo_map_str = "RxPdo")
    {
        auto xpath_str = "/EtherCATInfo/Descriptions/Devices/Device/" + pdo_map_str;
        // pugi::xpath_query ff;
        pugi::xpath_node_set rxpdos_nodes = doc.select_nodes(xpath_str.c_str());
        for (pugi::xpath_node node : rxpdos_nodes)
        {
            pugi::xml_node rxpdo = node.node();
            // std::cout << "Tool " << tool.attribute("Filename").value() << " has timeout " << tool.attribute("Timeout").as_int() << "\n";
            auto rpdo_idx = rxpdo.child("Index").text().get();
            // auto rpdo_idx = rxpdo.child("Index").value()//.as_uint();
            std::cout << "Index: " << rpdo_idx << "\n"; // why value() not work? //as_uint
            uint8_t tot_num = 0;
            for (pugi::xml_node entry : rxpdo.children("Entry"))
            {
                auto pdo_map_idx = entry.child("Index").text().get();
                auto pdo_map_subidx = entry.child("SubIndex").text().get();
                auto pdo_map_bit_len = entry.child("BitLen").text().get();
                std::cout << "Index: " << pdo_map_idx << ", SubIndex: " << pdo_map_subidx << ", BitLen: " << pdo_map_bit_len << "\n";
                tot_num += 1;

                // suprocess call
                if (!strcmp(pdo_map_subidx, ""))
                {
                    // strcmp ==0 if they are same.
                    // may exist empty 1 just for barrier
                    continue;
                    std::cout << "\n";
                }
                // sub-idx need add 1, COE pdo mapping strange requirement.
                uint32_t data = (hex_or_dec_str_to_int(pdo_map_idx) << 16 | (hex_or_dec_str_to_int(pdo_map_subidx)) << 8 | hex_or_dec_str_to_int(pdo_map_bit_len));
                std::vector<std::string> v{"sudo", "ethercat", "download",
                                           "--type", "uint32",
                                           Option("-a", program.get<std::string>("-a")).render(),
                                           Option("-p", program.get<std::string>("-p")).render(),
                                           cv(rpdo_idx), int2hex_string(tot_num), int2hex_string(data)};
                // std::cout << "cmd: " << join_cmd(v, ' ');
                execute_cmd(join_cmd(v, ' '));
                // throw;
            }
            std::vector<std::string> v{"sudo", "ethercat", "download",
                                       "--type", "uint32",
                                       Option("-a", program.get<std::string>("-a")).render(),
                                       Option("-p", program.get<std::string>("-p")).render(),
                                       cv(rpdo_idx), "0x00", int2hex_string(tot_num)};
            // std::cout << "cmd: " << join_cmd(v, ' ');
            execute_cmd(join_cmd(v, ' '));
            std::cout << "\n";
        }
    };
    ex_map("RxPdo");

    // txpdo
    ex_map("TxPdo");
    
    return 0;
}