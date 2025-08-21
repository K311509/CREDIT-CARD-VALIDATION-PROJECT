#include <iostream>
#include <string>
#include <winsock2.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// ---------- Luhn Algorithm ----------
bool isValidCard(const string &ccNumber) {
    if (ccNumber.empty()) return false;

    int len = ccNumber.length();
    int doubleEvenSum = 0;

    for (int i = len - 2; i >= 0; i -= 2) {
        int dbl = ((ccNumber[i] - '0') * 2);
        if (dbl > 9) {
            dbl = (dbl / 10) + (dbl % 10);
        }
        doubleEvenSum += dbl;
    }
    for (int i = len - 1; i >= 0; i -= 2) {
        doubleEvenSum += (ccNumber[i] - '0');
    }
    return (doubleEvenSum % 10 == 0);
}

// ---------- Card Type ----------
string getCardType(const string &ccNumber) {
    if (ccNumber.empty()) return "Unknown";
    if (ccNumber[0] == '4' && (ccNumber.size() == 13 || ccNumber.size() == 16))
        return "Visa";
    if (ccNumber.size() == 16 &&
        (ccNumber.substr(0, 2) >= "51" && ccNumber.substr(0, 2) <= "55"))
        return "MasterCard";
    if (ccNumber.size() == 15 &&
        (ccNumber.substr(0, 2) == "34" || ccNumber.substr(0, 2) == "37"))
        return "American Express";
    if (ccNumber.size() == 16 &&
        (ccNumber.substr(0, 4) == "6011" || ccNumber.substr(0, 2) == "65"))
        return "Discover";
    return "Unknown";
}

// ---------- URL Decode ----------
string urlDecode(const string &src) {
    string ret;
    char ch;
    int i, ii;
    for (i = 0; i < src.length(); i++) {
        if (src[i] == '%') {
            sscanf(src.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        } else if (src[i] == '+') {
            ret += ' ';
        } else {
            ret += src[i];
        }
    }
    return ret;
}

// ---------- Format Card Number ----------
string formatCardNumber(const string &digits) {
    string formatted;
    for (size_t i = 0; i < digits.size(); i++) {
        if (i > 0 && i % 4 == 0) {
            formatted += " ";
        }
        formatted += digits[i];
    }
    return formatted;
}

// ---------- Page Builder (injects result + logo + card number into static HTML) ----------
string buildMainPage(const string &result = "", const string &cardType = "", const string &cardNumber = "") {
    ifstream file("index.html");
    string html, line;
    while (getline(file, line)) {
        html += line + "\n";
    }
    file.close();

    // ---------- Inject result ----------
    string inject;
    if (!result.empty()) {
        inject = "Card Status: " + result;
        if (!cardType.empty()) {
            inject += " | Card Type: " + cardType;
        }
        inject = "<span style='color:" + string(result == "Valid" ? "green" : "red") +
                 ";font-weight:bold;'>" + inject + "</span>";
    }

    size_t pos = html.find("<div id=\"result\"");
    if (pos != string::npos) {
        size_t start = html.find(">", pos);
        size_t end = html.find("</div>", start);
        if (start != string::npos && end != string::npos) {
            html.replace(start + 1, end - start - 1, inject);
        }
    }

    // ---------- Inject logo ----------
    string logoTag;
    if (!result.empty() && result == "Valid") {
        string logoUrl;
        if (cardType == "Visa")
            logoUrl = "https://img.icons8.com/color/96/visa.png";
        else if (cardType == "MasterCard")
            logoUrl = "https://img.icons8.com/color/96/mastercard.png";
        else if (cardType == "American Express")
            logoUrl = "https://img.icons8.com/color/96/amex.png";
        else if (cardType == "Discover")
            logoUrl = "https://img.icons8.com/color/96/discover.png";
        else
            logoUrl = "https://img.icons8.com/ios-filled/96/bank-card-back-side.png";

        logoTag = "<img id=\"logo\" alt=\"Card logo\" src=\"" + logoUrl +
                  "\" style=\"display:block;width:80px;margin-top:15px;\">";
    } else {
        // Invalid or not entered → hide logo
        logoTag = "<img id=\"logo\" alt=\"Card logo\" style=\"display:none;width:80px;margin-top:15px;\">";
    }

    // Always replace <img id="logo"> tag
    size_t imgPos = html.find("<img id=\"logo\"");
    if (imgPos != string::npos) {
        size_t imgEnd = html.find(">", imgPos);
        if (imgEnd != string::npos) {
            html.replace(imgPos, imgEnd - imgPos + 1, logoTag);
        }
    }

    // ---------- Inject card number back into input ----------
    if (!cardNumber.empty()) {
        string formattedCard;
        for (size_t i = 0; i < cardNumber.size(); i++) {
            if (i > 0 && i % 4 == 0) formattedCard += " ";
            formattedCard += cardNumber[i];
        }

        size_t inputPos = html.find("id=\"ccn\"");
        if (inputPos != string::npos) {
            size_t valuePos = html.find("value=", inputPos);
            if (valuePos == string::npos) {
                size_t endInput = html.find(">", inputPos);
                if (endInput != string::npos) {
                    string insertValue = " value=\"" + formattedCard + "\"";
                    html.insert(endInput, insertValue);
                }
            } else {
                size_t quote1 = html.find("\"", valuePos + 6);
                size_t quote2 = html.find("\"", quote1 + 1);
                if (quote1 != string::npos && quote2 != string::npos) {
                    html.replace(quote1 + 1, quote2 - quote1 - 1, formattedCard);
                }
            }
        }
    }

    return html;
}


// ---------- Server ----------
int main() {
    WSADATA wsa;
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in server, client;
    int c;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8080);

    bind(serverSocket, (struct sockaddr *)&server, sizeof(server));
    listen(serverSocket, 3);

    cout << "Server running at http://localhost:8080" << endl;

    c = sizeof(struct sockaddr_in);

    while ((clientSocket = accept(serverSocket, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        char buffer[8192];
        int recv_size = recv(clientSocket, buffer, 8192, 0);
        if (recv_size > 0) {
            buffer[recv_size] = '\0';
            string request(buffer);

            string response, resultPage;

            if (request.find("POST") == 0) {
                // Extract body after the last \r\n\r\n
                size_t bodyPos = request.find("\r\n\r\n");
                string body = "";
                if (bodyPos != string::npos) {
                    body = request.substr(bodyPos + 4);
                }

                // Parse "card=xxxx"
                string cardNumber = "";
                size_t pos = body.find("card=");
                if (pos != string::npos) {
                    cardNumber = body.substr(pos + 5);
                }

                cardNumber = urlDecode(cardNumber);
                // Remove all non-digit characters
                cardNumber.erase(remove_if(cardNumber.begin(), cardNumber.end(), [](char c) { return !isdigit(c); }), cardNumber.end());

                cout << "Card number received: '" << cardNumber << "'" << endl;
                bool valid = isValidCard(cardNumber);
                string cardType = getCardType(cardNumber);
                resultPage = buildMainPage(valid ? "Valid" : "Invalid", cardType, cardNumber);
            } else {
                resultPage = buildMainPage();
            }

            response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + resultPage;
            send(clientSocket, response.c_str(), response.size(), 0);
        }
        closesocket(clientSocket);
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
