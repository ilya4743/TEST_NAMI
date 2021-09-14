#include <iostream>
#include<fstream>
#include <vector>
#include <winsock2.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma warning(disable : 4996)

using namespace std;

class DataPacket
{
public:
    int id;
    float speed;
    float x;
    float y;
    float w;
    float l;

    DataPacket()
    {
        id = speed = x = y = w = l = 0;
    }

    DataPacket(int id, float speed, float x, float y, float w, float l)
    {
        this->id = id;
        this->speed = speed;
        this->x = x;
        this->y = y;
        this->w = w;
        this->l = l;
    }

    DataPacket(const DataPacket& dp)
    {
        this->id = dp.id;
        this->speed = dp.speed;
        this->x = dp.x;
        this->y = dp.y;
        this->w = dp.w;
        this->l = dp.l;
    }

    friend ostream& operator<< (ostream& out, const DataPacket& data)
    {
        out << "id = " << data.id << endl;
        out << "speed = " << data.speed << endl;
        out << "x = " << data.x << endl;
        out << "y = " << data.y << endl;
        out << "width = " << data.w << endl;
        out << "length = " << data.l << endl;
        return out;
    }
};

int to_int(char* c)
{
    BYTE buffer[4] = { c[3],c[2],c[1],c[0] };
    int value;
    memcpy(&value, buffer, sizeof(int));
    return value;
}

float to_float(char* c)
{
    BYTE buffer[4] = { c[0],c[1],c[2],c[3] };
    return *reinterpret_cast<float*>(buffer);
}

void ParseDataPacket(vector<DataPacket>& data,char* data_pack)
{
    int id = to_int(data_pack);
    float speed = to_float(data_pack + 4);
    float x = to_float(data_pack + 8);
    float y = to_float(data_pack + 12);
    float w = to_float(data_pack + 16);
    float l = to_float(data_pack + 20);
    data.push_back(DataPacket(id, speed, x, y, w, l));
}

static uint32_t crc32(uint32_t crc, unsigned char* buf, size_t len)
{
    int k;
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
    }
    return ~crc;
}

int main()
{
    vector<DataPacket>data; //вектор дата пакетов одного инфо пакета
    WSADATA wsaData;    
    int packets = 0;        //счётчик инфо пакетов
    int iResult;
    unsigned int crc = 0;   //вычисленная crc инфо пакета
    char head[3];           //голова инфо пакета
    char data_pack[24];     //1 дата пакет
    char tail[6];           //хвост инфо пакета
    int count_data_packet;  //кол-во дата пакетов
    int k = 0;
    ofstream out;
    out.open("out.txt", std::ofstream::out | std::ofstream::trunc);
    out.close();

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    SOCKET ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    SOCKADDR_IN s;
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_port = htons(36215);

    s.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    connect(ConnectSocket, (const sockaddr*)&s, sizeof(s));
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        printf("Unable to connect to server: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    while (true)
    {
        //Read Head
        iResult = recv(ConnectSocket, head, 3, 0);
        if (iResult > 0)
        {
            //printf("Bytes received: %d\n", iResult);
            //for (int i = 0; i < 3; i++)
            //    cout << int(unsigned char(head[i])) << ' ';
            //cout << endl;
            count_data_packet = unsigned char(head[2]);
            unsigned char* buf_crc = new unsigned char[count_data_packet * 24 + 1];
            buf_crc[0] = count_data_packet;

            //Read Data Packet
            data.reserve(count_data_packet);
            do 
            {
                iResult = recv(ConnectSocket, data_pack, sizeof(data_pack), 0);
                if (iResult > 0)
                {
                    //printf("Bytes received: %d\n", iResult);
                    for (int i = 0; i < 24; i++)
                    {
                        //    cout << int(unsigned char(data_pack[i])) << ' ';
                        buf_crc[i + 24 * k + 1] = data_pack[i];
                    }
                    //cout << endl;

                    ParseDataPacket(data, data_pack);

                    //cout << data[k] << endl;
                    count_data_packet--;
                    k++;
                }
                else if (iResult == 0)
                { 
                    printf("Connection closed\n");
                    break;
                }
                else
                { 
                    printf("recv failed: %d\n", WSAGetLastError());
                    break;
                }
            } while (count_data_packet != 0);
            count_data_packet = unsigned char(head[2]);
            crc = crc32(0, (buf_crc), count_data_packet * 24 + 1);
            k = 0;
            delete[]buf_crc;

            //Read Tail
            iResult = recv(ConnectSocket, tail, 6, 0);
            unsigned int crc32 = 0;
            if (iResult > 0)
            {
                //printf("Bytes received: %d\n", iResult);
                crc32 = to_int(tail);
                //for (int i = 0; i < 6; i++)
                //    cout << int(unsigned char(tail[i])) << ' ';
                //cout << endl;
            }
            else if (iResult == 0)
                printf("Connection closed\n");
            else
                printf("recv failed: %d\n", WSAGetLastError());

            //Analize
            if (crc == crc32)   //CRC OK!
            {
                packets++;
                vector<DataPacket> LeftAuto;
                LeftAuto.push_back(data[0]);
                for (int i = 1; i < data.size(); i++)
                    if (data[i].x < -(data[0].w / 2.0))
                        LeftAuto.push_back(data[i]);
                data.clear();
                //for (int i = 1; i < LeftAuto.size(); i++)
                //    cout << LeftAuto[i] << endl;

                //cout << "Perestroenie" << endl<<endl;
                //ситуация после перестроения
                for (int i = 0; i < LeftAuto.size(); i++)
                    LeftAuto[i].y += (LeftAuto[i].speed - LeftAuto[0].speed) * 3;
                //for (int i = 1; i < LeftAuto.size(); i++)
                //    cout <<  LeftAuto[i] << endl;

                DataPacket front, back;
                back.y = INT_MIN;
                front.y = INT_MAX;

                for (int i = 0; i < LeftAuto.size(); i++)
                {
                    if (LeftAuto[i].y < LeftAuto[0].y && back.y < LeftAuto[i].y) back = LeftAuto[i];
                    if (LeftAuto[i].y > LeftAuto[0].y && front.y > LeftAuto[i].y) front = LeftAuto[i];
                }
                float y1, y2;
                out.open("out.txt", ios_base::out | ios_base::app);
                out<< packets << ".\t";
                //cout << packets << ".\t";
                if (back.y != INT_MIN && front.y != INT_MAX)
                {
                    y1 = back.y + LeftAuto[0].l / 2 + back.l / 2 + back.speed;
                    y2 = front.y - LeftAuto[0].l / 2 - front.l / 2 - LeftAuto[0].speed;
                    if (y1 > y2)
                    {
                        //cout << "No points" << endl;
                        out << "No points\n";
                    }
                    else
                    {
                        //cout << "Start point(-3; " << y1 << ")\t";
                        //cout << "End point(-3; " << y2 << ')' << endl;
                        out << "Start point(-3; " << y1 << ")\t";
                        out << "End point(-3; " << y2 << ")\n";
                    }
                }
                else
                {
                    if (back.y == INT_MIN)
                    {
                        //cout << "Start point(-3; -oo)\t";
                        out << "Start point(-3; -oo)\t";
                    }
                    else
                    {
                        y1 = back.y + LeftAuto[0].l / 2 + back.l / 2 + back.speed;
                        //cout << "Start point(-3; " << y1 << ")\t";
                        out << "Start point(-3; " << y1 << ")\t";
                    }

                    if (front.y == INT_MAX)
                    {
                        //cout << "End point(-3; +oo)" << endl;
                        out << "End point(-3; +oo)\n";
                    }
                    else
                    {
                        y2 = front.y - LeftAuto[0].l / 2 - front.l / 2 - LeftAuto[0].speed;
                        //cout << "End point(-3; " << y2 << ')' << endl;
                        out << "End point(-3; " << y2 << ")\n";
                    }
                }
                out.close();
                LeftAuto.clear();
            }
        }
        else if (iResult == 0)
        {
            cout<<"Connection closed! "<<packets<<" packets received.\nOpen out.txt\n"; 
            break;
        }
        else
        { 
            printf("recv failed: %d\n", WSAGetLastError()); 
            break;
        }
    }
    closesocket(ConnectSocket);
    WSACleanup();
    system("PAUSE");
    return 0;
}