#include <bits/stdc++.h>
using namespace std;

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <map>

namespace fs = std::filesystem;

// 分割字符串
vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// 定义选手结构体
struct Player {
    string affiliation;
    string year;
    string name;
    string gender;
    string specialMark;
    vector<pair<string, string>> affiliations; // 从属信息，格式：{起始年份-终止年份, 从属}
    vector<pair<string, vector<string>>> personalContests; // 个人赛信息，格式：{比赛名, {比赛年份, 赛别, 赛级, 奖, 排名, 分数}}
    vector<pair<string, vector<string>>> groupContests; // 团体赛信息，格式：{比赛名, {比赛年份, 赛别, 赛级, 奖, 排名, 分数}}
    int score = 0; // 总分
    int rank = 0;  // 排名
};

// 定义比赛结构体
struct Contest {
    string name;
    string year;
    string category;
    string level;
    string award;
    string rank;
    string score;
    bool isSpecial; // 标记是否为特殊比赛
    vector<string> players; // 参赛选手
    vector<string> teachers; // 指导老师
};

// 定义老师结构体
struct Teacher {
    string affiliation;
    string name;
    string specialMark;
    vector<pair<string, string>> affiliations; // 从属信息，格式：{起始年份-终止年份, 从属}
    vector<pair<string, vector<string>>> contests; // 指导比赛信息，格式：{比赛名, {比赛年份, 赛别, 赛级, 奖, 排名, 分数, 选手届, 选手名}}
};

// 比较函数，用于排序比赛信息（从旧到新，按赛级、奖级、赛别）
bool compareContests(const pair<string, vector<string>>& a, const pair<string, vector<string>>& b) {
    static map<string, int> levelOrder = {{"国际", 1}, {"国级", 2}, {"省级", 3}, {"市级", 4}, {"校级", 5}, {"其他", 6}, {"-", 7}};
    static map<string, int> awardOrder = {{"特等奖", 1}, {"一等奖", 2}, {"二等奖", 3}, {"三等奖", 4}, {"优秀奖", 5}, {"其他", 6}, {"-", 7}};
    static map<string, int> categoryOrder = {{"数学", 1}, {"物理", 2}, {"化学", 3}, {"生物", 4}, {"信息", 5}, {"英语", 6}, {"其他", 7}, {"-", 8}};
    
    if (a.second[0] != b.second[0]) {
        return a.second[0] < b.second[0]; // 按比赛年份从旧到新排
    }
    if (levelOrder[a.second[2]] != levelOrder[b.second[2]]) {
        return levelOrder[a.second[2]] < levelOrder[b.second[2]]; // 按赛级排序
    }
    if (awardOrder[a.second[3]] != awardOrder[b.second[3]]) {
        return awardOrder[a.second[3]] < awardOrder[b.second[3]]; // 按奖级排序
    }
    return categoryOrder[a.second[1]] < categoryOrder[b.second[1]]; // 按赛别排序
}

// 比较函数，用于排序比赛中的选手排名
bool comparePlayersInContest(const vector<string>& a, const vector<string>& b) {
    if (a[4] != b[4]) {
        return a[4] < b[4]; // 按排名排序
    }
    if (a[5] != b[5]) {
        return a[5] > b[5]; // 按得分排序
    }
    static map<string, int> awardOrder = {{"特等奖", 1}, {"一等奖", 2}, {"二等奖", 3}, {"三等奖", 4}, {"优秀奖", 5}, {"其他", 6}, {"-", 7}};
    return awardOrder[a[3]] < awardOrder[b[3]]; // 按奖级排序
}

// 合并从属信息
void mergeAffiliations(vector<pair<string, string>>& affiliations) {
    sort(affiliations.begin(), affiliations.end(), [](const auto& a, const auto& b) {
        return stoi(a.first.substr(0, 4)) < stoi(b.first.substr(0, 4));
    });
    vector<pair<string, string>> merged;
    for (const auto& aff : affiliations) {
        if (merged.empty() || merged.back().second != aff.second) {
            merged.push_back(aff);
        } else {
            int endYear = max(stoi(merged.back().first.substr(5, 4)), stoi(aff.first.substr(5, 4)));
            merged.back().first = merged.back().first.substr(0, 5) + to_string(endYear);
        }
    }
    affiliations = merged;
}

// 生成 Markdown 文件
void generateMarkdown(const string& path, const string& title, const string& content, int order) {
    ofstream file(path);
    if (file.is_open()) {
        file << "---" << endl;
        file << "title: " << title << endl;
        file << "order: " << order << endl;
        file << "---" << endl;
        file << content;
        file.close();
    }
}

// 计算得分
int calculateScore(const vector<string>& contestInfo, bool isSpecial) {
    static map<string, int> levelScore = {{"国际", 10}, {"国级", 8}, {"省级", 6}, {"市级", 4}, {"校级", 2}, {"其他", 1}, {"-", 1}};
    static map<string, int> awardScore = {{"特等奖", 5}, {"一等奖", 4}, {"二等奖", 3}, {"三等奖", 2}, {"优秀奖", 1}, {"其他", 1}, {"-", 1}};
    int baseScore = levelScore[contestInfo[2]] + awardScore[contestInfo[3]];
    return baseScore + (isSpecial ? 3 : 0);
}

// 生成选手 Markdown 文件
void generatePlayerMarkdown(const Player& player, const map<string, Contest>& contests, const map<string, Teacher>& teachers) {
    string filename;
    if (!player.specialMark.empty() && player.specialMark != "-") {
        filename = player.name + "(" + player.specialMark + ").md";
    } else if (!player.gender.empty()) {
        // 检查是否有同名同年同特殊标记但不同性别的选手
        bool needGenderDistinction = false;
        for (const auto& [key, otherContest] : contests) {
            for (const string& playerPath : otherContest.players) {
                size_t pos = playerPath.find_last_of('/');
                string otherPlayerYear = playerPath.substr(0, pos);
                string otherPlayerNameWithMark = playerPath.substr(pos + 1);
                
                // 提取名字和可能的括号内容
                string otherPlayerName;
                string otherPlayerMark;
                size_t bracketPos = otherPlayerNameWithMark.find('(');
                if (bracketPos != string::npos) {
                    otherPlayerName = otherPlayerNameWithMark.substr(0, bracketPos);
                    otherPlayerMark = otherPlayerNameWithMark.substr(bracketPos + 1, otherPlayerNameWithMark.length() - bracketPos - 2);
                } else {
                    otherPlayerName = otherPlayerNameWithMark;
                    otherPlayerMark = "";
                }
                
                // 检查是否需要区分性别
                if (otherPlayerYear == player.year && 
                    otherPlayerName == player.name && 
                    otherPlayerMark == player.specialMark &&
                    otherPlayerNameWithMark != (player.name + "(" + player.gender + ")")) {
                    needGenderDistinction = true;
                    break;
                }
            }
            if (needGenderDistinction) break;
        }
        
        if (needGenderDistinction) {
            filename = player.name + "(" + player.gender + ").md";
        } else {
            filename = player.name + ".md";
        }
    } else {
        filename = player.name + ".md";
    }
    
    string path = "players/" + player.year + "/" + filename;
    fs::create_directories(fs::path(path).parent_path());

    string content = "## 个人信息\n";
    content += "| 基本信息 | 详情 |\n";
    content += "| --- | --- |\n";
    content += "| 姓名 | " + player.name + " |\n";
    content += "| 届别 | [" + player.year + "](/players/" + player.year + "/) |\n";
    content += "| 排名 | [" + to_string(player.rank) + "](/share/得分计算.html) |\n";
    content += "| 总分 | [" + to_string(player.score) + "](/share/得分计算.html) |\n";
    if (!player.gender.empty()) content += "| 性别 | " + player.gender + " |\n";
    if (!player.specialMark.empty() && player.specialMark != "-") content += "| 特殊标识 | " + player.specialMark + " |\n";
    
    content += "| 从属关系 | ";
    for (size_t i = 0; i < player.affiliations.size(); i++) {
        content += player.affiliations[i].second + " (" + player.affiliations[i].first + ")";
        if (i < player.affiliations.size() - 1) content += "<br>";
    }
    content += " |\n";

    // 为每个比赛收集该选手的指导老师
    map<string, vector<string>> contestTeachers;
    
    // 对个人赛和团体赛信息进行排序
    vector<pair<string, vector<string>>> sortedPersonalContests = player.personalContests;
    vector<pair<string, vector<string>>> sortedGroupContests = player.groupContests;
    sort(sortedPersonalContests.begin(), sortedPersonalContests.end(), compareContests);
    sort(sortedGroupContests.begin(), sortedGroupContests.end(), compareContests);

    // 处理个人赛
    for (const auto& contest : sortedPersonalContests) {
        string contestKey = contest.second[0] + "-" + contest.first;
        
        // 遍历所有老师，查找指导过该选手参加此比赛的老师
        for (const auto& [teacherKey, teacher] : teachers) {
            for (const auto& teacherContest : teacher.contests) {
                if (teacherContest.first == contest.first && 
                    teacherContest.second[0] == contest.second[0] && 
                    teacherContest.second[7] == player.name && 
                    teacherContest.second[6] == player.year) {
                    
                    // 找到匹配的老师，添加到该比赛的指导老师列表
                    string teacherPath = teacher.specialMark == "-" ? 
                        teacher.name : teacher.name + (teacher.specialMark.empty() ? "" : "(" + teacher.specialMark + ")");
                    contestTeachers[contestKey].push_back(teacherPath);
                    break;
                }
            }
        }
    }
    
    // 处理团体赛
    for (const auto& contest : sortedGroupContests) {
        string contestKey = contest.second[0] + "-" + contest.first;
        
        // 遍历所有老师，查找指导过该选手参加此比赛的老师
        for (const auto& [teacherKey, teacher] : teachers) {
            for (const auto& teacherContest : teacher.contests) {
                if (teacherContest.first == contest.first && 
                    teacherContest.second[0] == contest.second[0] && 
                    teacherContest.second[7] == player.name && 
                    teacherContest.second[6] == player.year) {
                    
                    // 找到匹配的老师，添加到该比赛的指导老师列表
                    string teacherPath = teacher.specialMark == "-" ? 
                        teacher.name : teacher.name + (teacher.specialMark.empty() ? "" : "(" + teacher.specialMark + ")");
                    contestTeachers[contestKey].push_back(teacherPath);
                    break;
                }
            }
        }
    }

    // 添加比赛二级标题
    if (!sortedPersonalContests.empty() || !sortedGroupContests.empty()) {
        content += "\n## 比赛\n";
        
        // 添加个人赛三级标题
        if (!sortedPersonalContests.empty()) {
            content += "\n### 个人赛 (" + to_string(sortedPersonalContests.size()) + "场)\n";
            content += "| 比赛名 | 比赛年份 | 赛别 | 赛级 | 奖 | 排名 | 分数 | 指导老师 |\n";
            content += "| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |\n";
            for (const auto& contest : sortedPersonalContests) {
                string contestKey = contest.second[0] + "-" + contest.first;
                string teachersStr = "";
                
                // 获取该比赛的指导老师列表
                if (contestTeachers.find(contestKey) != contestTeachers.end()) {
                    const vector<string>& teachersList = contestTeachers[contestKey];
                    for (size_t i = 0; i < teachersList.size(); i++) {
                        teachersStr += "[" + teachersList[i] + "](/teachers/" + teachersList[i] + ".html)";
                        if (i < teachersList.size() - 1) teachersStr += ", ";
                    }
                }

                string realContestName = contest.first;
                bool isSpecial = realContestName[0] == '*';
                if (isSpecial) {
                    realContestName = realContestName.substr(1);
                }

                string contestLink = "[" + realContestName + "](/games/" + contest.second[0] + "/" + realContestName + ".md)";
                if (isSpecial) {
                    contestLink += "[*](/share/特殊比赛.html)";
                }

                content += "| " + contestLink + " | " + contest.second[0] + " | " + contest.second[1] + " | " + contest.second[2] + " | " + contest.second[3] + " | " + contest.second[4] + " | " + contest.second[5] + " | " + teachersStr + " |\n";
            }
        }

        // 添加团体赛三级标题
        if (!sortedGroupContests.empty()) {
            content += "\n### 团体赛 (" + to_string(sortedGroupContests.size()) + "场)\n";
            content += "| 比赛名 | 比赛年份 | 赛别 | 赛级 | 奖 | 排名 | 分数 | 指导老师 |\n";
            content += "| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |\n";
            for (const auto& contest : sortedGroupContests) {
                string contestKey = contest.second[0] + "-" + contest.first;
                string teachersStr = "";
                
                // 获取该比赛的指导老师列表
                if (contestTeachers.find(contestKey) != contestTeachers.end()) {
                    const vector<string>& teachersList = contestTeachers[contestKey];
                    for (size_t i = 0; i < teachersList.size(); i++) {
                        teachersStr += "[" + teachersList[i] + "](/teachers/" + teachersList[i] + ".html)";
                        if (i < teachersList.size() - 1) teachersStr += ", ";
                    }
                }

                string realContestName = contest.first;
                bool isSpecial = realContestName[0] == '*';
                if (isSpecial) {
                    realContestName = realContestName.substr(1);
                }

                string contestLink = "[" + realContestName + "](/games/" + contest.second[0] + "/" + realContestName + ".md)";
                if (isSpecial) {
                    contestLink += "[*](/share/特殊比赛.html)";
                }

                content += "| " + contestLink + " | " + contest.second[0] + " | " + contest.second[1] + " | " + contest.second[2] + " | " + contest.second[3] + " | " + contest.second[4] + " | " + contest.second[5] + " | " + teachersStr + " |\n";
            }
        }
    }

    generateMarkdown(path, filename.substr(0, filename.length() - 3), content, player.rank);
}

// 生成比赛 Markdown 文件
void generateContestMarkdown(const Contest& contest, const map<string, Teacher>& teachers) {
    string realContestName = contest.name;
    bool isSpecial = realContestName[0] == '*';
    if (isSpecial) {
        realContestName = realContestName.substr(1);
    }

    string path = "games/" + contest.year + "/" + realContestName + ".md";
    fs::create_directories(fs::path(path).parent_path());

    string content = "## 比赛信息\n";
    content += "| 比赛名 | 比赛年份 | 赛别 | 赛级 |\n";
    content += "| ---- | ---- | ---- | ---- |\n";
    content += "| " + (isSpecial ? "**" + realContestName + "**" : realContestName) + " | " + contest.year + " | " + contest.category + " | " + contest.level + " |\n";
    
    // 特殊比赛标注
    if (isSpecial) {
        content += "\n**特别说明**：此比赛为[特殊比赛](/share/特殊比赛.html)，其成绩在总分计算中会额外加分。\n";
    }

    content += "\n## 参赛选手\n";
    content += "| 选手名 | 届 | 奖 | 排名 | 分数 | 指导老师 |\n";
    content += "| ---- | ---- | ---- | ---- | ---- | ---- |\n";
    
    // 为每个选手收集指导老师信息
    map<string, vector<string>> playerTeachers;
    
    // 收集选手的比赛信息
    map<string, vector<string>> playerInfo;
    
    // 遍历所有老师，查找指导过该比赛的老师及其指导的选手
    for (const auto& [teacherKey, teacher] : teachers) {
        for (const auto& teacherContest : teacher.contests) {
            if (teacherContest.first == contest.name && 
                teacherContest.second[0] == contest.year) {
                
                string playerKey = teacherContest.second[6] + "/" + teacherContest.second[7];
                string teacherPath = teacher.specialMark == "-" ? 
                    teacher.name : teacher.name + (teacher.specialMark.empty() ? "" : "(" + teacher.specialMark + ")");
                
                playerTeachers[playerKey].push_back(teacherPath);
                
                // 存储选手在这场比赛中的信息，包括原始分数
                playerInfo[playerKey] = vector<string>{teacherContest.second[3], teacherContest.second[4], teacherContest.second[5]};
            }
        }
    }
    
    // 按排名、得分、奖级排序选手
    vector<vector<string>> sortedPlayers;
    for (const string& playerPath : contest.players) {
        size_t pos = playerPath.find_last_of('/');
        string playerName = playerPath.substr(pos + 1);
        string playerYear = playerPath.substr(0, pos);
        
        string playerKey = playerYear + "/" + playerName;
        string award = "-";
        string rank = "-";
        string score = "-";
        
        if (playerInfo.find(playerKey) != playerInfo.end()) {
            award = playerInfo[playerKey][0];
            rank = playerInfo[playerKey][1];
            score = playerInfo[playerKey][2];
        }
        
        sortedPlayers.push_back({playerPath, playerYear, playerName, award, rank, score});
    }
    
    sort(sortedPlayers.begin(), sortedPlayers.end(), [](const vector<string>& a, const vector<string>& b) {
        if (a[4] != b[4]) {
            return a[4] < b[4]; // 按排名排序
        }
        if (a[5] != b[5]) {
            return a[5] > b[5]; // 按得分排序
        }
        static map<string, int> awardOrder = {{"特等奖", 1}, {"一等奖", 2}, {"二等奖", 3}, {"三等奖", 4}, {"优秀奖", 5}, {"其他", 6}, {"-", 7}};
        return awardOrder[a[3]] < awardOrder[b[3]]; // 按奖级排序
    });
    
    for (const auto& playerData : sortedPlayers) {
        string playerPath = playerData[0];
        string playerYear = playerData[1];
        string playerName = playerData[2];
        string award = playerData[3];
        string rank = playerData[4];
        string score = playerData[5];
        
        // 解析选手名字中的特殊标记
        string displayName = playerName;
        size_t bracketPos = playerName.find('(');
        if (bracketPos != string::npos) {
            string mark = playerName.substr(bracketPos + 1, playerName.length() - bracketPos - 2);
            if (mark != "-") {
                displayName = playerName;
            } else {
                displayName = playerName.substr(0, bracketPos);
            }
        }
        
        // 获取指导老师信息
        string teachersStr = "";
        string playerKey = playerYear + "/" + playerName;
        if (playerTeachers.find(playerKey) != playerTeachers.end()) {
            const vector<string>& teachersList = playerTeachers[playerKey];
            for (size_t i = 0; i < teachersList.size(); i++) {
                teachersStr += "[" + teachersList[i] + "](/teachers/" + teachersList[i] + ".html)";
                if (i < teachersList.size() - 1) teachersStr += ", ";
            }
        }
        
        content += "| [" + displayName + "](/players/" + playerYear + "/" + playerName + ".md) | [" + playerYear + "](/players/" + playerYear + "/) | " + award + " | " + rank + " | " + score + " | " + teachersStr + " |\n";
    }

    generateMarkdown(path, realContestName, content, 0); // 比赛文件 order 设为 0
}
// 生成老师 Markdown 文件
void generateTeacherMarkdown(const Teacher& teacher) {
    string filename;
    if (!teacher.specialMark.empty() && teacher.specialMark != "-") {
        filename = teacher.name + "(" + teacher.specialMark + ").md";
    } else {
        filename = teacher.name + ".md";
    }
    
    string path = "teachers/" + filename;
    fs::create_directories(fs::path(path).parent_path());

    string content = "## 老师信息\n";
    content += "| 从属 | 名字 |\n";
    content += "| ---- | ---- |\n";
    for (const auto& aff : teacher.affiliations) {
        content += "| " + aff.second + " (" + aff.first + ") | " + teacher.name + " |\n";
    }

    content += "\n## 指导比赛信息\n";

    vector<pair<string, vector<string>>> personalContests;
    vector<pair<string, vector<string>>> groupContests;

    // 区分个人赛和团队赛
    for (const auto& contest : teacher.contests) {
        string contestName = contest.first;
        vector<string> contestInfo = contest.second;
        string playerName = contestInfo[7];
        string playerYear = contestInfo[6];
        string key = playerYear + "-" + playerName;

        // 假设选手信息里可以通过人数判断是否为团队赛
        bool isGroupContest = false;
        // 这里简单模拟，实际情况可能需要根据数据格式调整判断逻辑
        // 比如从其他地方获取团队赛相关信息
        if (contestInfo.size() > 8) { 
            isGroupContest = true;
        }

        if (isGroupContest) {
            groupContests.emplace_back(contestName, contestInfo);
        } else {
            personalContests.emplace_back(contestName, contestInfo);
        }
    }

    // 添加个人赛表格
    if (!personalContests.empty()) {
        content += "\n### 个人赛 (" + to_string(personalContests.size()) + "场)\n";
        content += "| 选手名 | 比赛名 | 比赛年份 | 赛别 | 赛级 | 奖 | 选手得分 | 选手届 |\n";
        content += "| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |\n";
        for (const auto& contest : personalContests) {
            string realContestName = contest.first;
            bool isSpecial = realContestName[0] == '*';
            if (isSpecial) {
                realContestName = realContestName.substr(1);
            }

            string contestLink = "[" + realContestName + "](/games/" + contest.second[0] + "/" + realContestName + ".md)";
            if (isSpecial) {
                contestLink += "[*](/share/特殊比赛.html)";
            }

            content += "| [" + contest.second[7] + "](/players/" + contest.second[6] + "/" + contest.second[7] + ".md) | " + contestLink + " | " + contest.second[0] + " | " + contest.second[1] + " | " + contest.second[2] + " | " + contest.second[3] + " | " + contest.second[5] + " | [" + contest.second[6] + "](/players/" + contest.second[6] + "/) |\n";
        }
    }

    // 添加团队赛表格
    if (!groupContests.empty()) {
        content += "\n### 团队赛 (" + to_string(groupContests.size()) + "场)\n";
        content += "| 选手名 | 比赛名 | 比赛年份 | 赛别 | 赛级 | 奖 | 选手得分 | 选手届 |\n";
        content += "| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |\n";
        for (const auto& contest : groupContests) {
            string realContestName = contest.first;
            bool isSpecial = realContestName[0] == '*';
            if (isSpecial) {
                realContestName = realContestName.substr(1);
            }

            string contestLink = "[" + realContestName + "](/games/" + contest.second[0] + "/" + realContestName + ".md)";
            if (isSpecial) {
                contestLink += "[*](/share/特殊比赛.html)";
            }

            content += "| [" + contest.second[7] + "](/players/" + contest.second[6] + "/" + contest.second[7] + ".md) | " + contestLink + " | " + contest.second[0] + " | " + contest.second[1] + " | " + contest.second[2] + " | " + contest.second[3] + " | " + contest.second[5] + " | [" + contest.second[6] + "](/players/" + contest.second[6] + "/) |\n";
        }
    }

    generateMarkdown(path, filename.substr(0, filename.length() - 3), content, 0); // 老师文件 order 设为 0
}    

// 解析一行信息
void parseLine(const string& line, map<string, Player>& players, map<string, Contest>& contests, map<string, Teacher>& teachers) {
    istringstream iss(line);
    vector<string> tokens;
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() < 13) {
        cerr << "Error parsing line: " << line << endl;
        return;
    }

    vector<string> affiliations = split(tokens[0], ',');
    vector<string> years = split(tokens[1], ',');
    vector<string> names = split(tokens[2], ',');
    vector<string> genders = split(tokens[3], ',');
    string specialMark = tokens[4];
    string contestName = tokens[5];
    string contestYear = tokens[6];
    string contestCategory = tokens[7];
    string contestLevel = tokens[8];
    string contestAward = tokens[9];
    string contestRank = tokens[10];
    string contestScore = tokens[11]; // 原始分数
    vector<string> teacherAffiliations = split(tokens[12], ',');
    vector<string> teacherNames = split(tokens[13], ',');
    vector<string> teacherSpecialMarks = split(tokens[14], ',');

    bool isGroupContest = names.size() > 1;
    bool isSpecial = contestName[0] == '*';

    vector<string> contestInfo = {contestYear, contestCategory, contestLevel, contestAward, contestRank, contestScore};
    int contestScoreValue = calculateScore(contestInfo, isSpecial);

    for (size_t i = 0; i < names.size(); ++i) {
        // 使用更精确的选手键：届+名字+特殊标记+性别
        string playerKey = years[i] + "-" + names[i] + "-" + specialMark + "-" + (i < genders.size() ? genders[i] : "");
        if (players.find(playerKey) == players.end()) {
            players[playerKey] = {
                i < affiliations.size() ? affiliations[i] : "-", 
                years[i], 
                names[i], 
                i < genders.size() ? genders[i] : "", 
                specialMark, {}, {}, {}, 0, 0
            };
        }
        players[playerKey].affiliations.emplace_back(contestYear + "-" + contestYear, i < affiliations.size() ? affiliations[i] : "-");
        players[playerKey].score += contestScoreValue; // 累加总分

        if (isGroupContest) {
            players[playerKey].groupContests.emplace_back(contestName, contestInfo);
        } else {
            players[playerKey].personalContests.emplace_back(contestName, contestInfo);
        }
    }

    string contestKey = contestYear + "-" + contestName;
    if (contests.find(contestKey) == contests.end()) {
        contests[contestKey] = {
            contestName, 
            contestYear, 
            contestCategory, 
            contestLevel, 
            contestAward, 
            contestRank, 
            contestScore, // 存储原始分数，而不是计算后的分数
            isSpecial, {}, {}
        };
    }
    for (size_t i = 0; i < names.size(); ++i) {
        // 使用更精确的选手键：届+名字+特殊标记+性别
        string playerKey = years[i] + "-" + names[i] + "-" + specialMark + "-" + (i < genders.size() ? genders[i] : "");
        
        // 确定选手的文件名
        string playerFileName;
        if (!specialMark.empty() && specialMark != "-") {
            playerFileName = names[i] + "(" + specialMark + ")";
        } else if (!players[playerKey].gender.empty()) {
            // 检查是否需要区分性别
            bool needGenderDistinction = false;
            for (const auto& [key, otherContest] : contests) {
                for (const string& playerPath : otherContest.players) {
                    size_t pos = playerPath.find_last_of('/');
                    string otherPlayerYear = playerPath.substr(0, pos);
                    string otherPlayerNameWithMark = playerPath.substr(pos + 1);
                    
                    // 提取名字和可能的括号内容
                    string otherPlayerName;
                    string otherPlayerMark;
                    size_t bracketPos = otherPlayerNameWithMark.find('(');
                    if (bracketPos != string::npos) {
                        otherPlayerName = otherPlayerNameWithMark.substr(0, bracketPos);
                        otherPlayerMark = otherPlayerNameWithMark.substr(bracketPos + 1, otherPlayerNameWithMark.length() - bracketPos - 2);
                    } else {
                        otherPlayerName = otherPlayerNameWithMark;
                        otherPlayerMark = "";
                    }
                    
                    // 检查是否需要区分性别
                    if (otherPlayerYear == years[i] && 
                        otherPlayerName == names[i] && 
                        otherPlayerMark == specialMark &&
                        otherPlayerNameWithMark != (names[i] + "(" + players[playerKey].gender + ")")) {
                        needGenderDistinction = true;
                        break;
                    }
                }
                if (needGenderDistinction) break;
            }
            
            if (needGenderDistinction) {
                playerFileName = names[i] + "(" + players[playerKey].gender + ")";
            } else {
                playerFileName = names[i];
            }
        } else {
            playerFileName = names[i];
        }
        
        string playerPath = years[i] + "/" + playerFileName;
        contests[contestKey].players.push_back(playerPath);
    }

    for (size_t i = 0; i < teacherNames.size(); ++i) {
        // 使用更精确的老师键：名字+特殊标记
        string teacherKey = teacherNames[i] + "-" + (i < teacherSpecialMarks.size() ? teacherSpecialMarks[i] : "");
        if (teachers.find(teacherKey) == teachers.end()) {
            teachers[teacherKey] = {
                i < teacherAffiliations.size() ? teacherAffiliations[i] : "-", 
                teacherNames[i], 
                i < teacherSpecialMarks.size() ? teacherSpecialMarks[i] : "", 
                {}, {}
            };
        }
        teachers[teacherKey].affiliations.emplace_back(contestYear + "-" + contestYear, i < teacherAffiliations.size() ? teacherAffiliations[i] : "-");

        for (size_t j = 0; j < names.size(); ++j) {
            vector<string> contestInfo = {contestYear, contestCategory, contestLevel, contestAward, contestRank, contestScore, years[j], names[j]};
            teachers[teacherKey].contests.emplace_back(contestName, contestInfo);
        }
        string teacherPath = teachers[teacherKey].name;
        if (!teachers[teacherKey].specialMark.empty() && teachers[teacherKey].specialMark != "-") {
            teacherPath += "(" + teachers[teacherKey].specialMark + ")";
        }
        contests[contestKey].teachers.push_back(teacherPath);
    }
}

// 检查是否有举办年份相同且比赛名字只差 * 号的情况
bool checkDuplicateContests(const map<string, Contest>& contests) {
    map<string, bool> normalContests;
    for (const auto& [key, contest] : contests) {
        string realContestName = contest.name;
        if (contest.isSpecial) {
            realContestName = realContestName.substr(1);
        }
        string contestKey = contest.year + "-" + realContestName;
        if (contest.isSpecial) {
            if (normalContests[contestKey]) {
                cerr << "Error: Duplicate contests with and without * in year " << contest.year << " for contest " << realContestName << endl;
                return true;
            }
        } else {
            normalContests[contestKey] = true;
        }
    }
    return false;
}

int main() {
    // 清空 players 、games 、teachers 文件夹
    fs::remove_all("players");
    fs::remove_all("games");
    fs::remove_all("teachers");

    map<string, Player> players;
    map<string, Contest> contests;
    map<string, Teacher> teachers;

    ifstream file("data.txt");
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            // 忽略以 '#' 开头的行和空行
            if (line.empty() || line[0] == '#') continue;
            // 去除行末空格
            line.erase(line.find_last_not_of(" \t") + 1);
            parseLine(line, players, contests, teachers);
        }
        file.close();
    } else {
        cerr << "Unable to open file data.txt" << endl;
        return 1;
    }

    // 检查是否有举办年份相同且比赛名字只差 * 号的情况
    if (checkDuplicateContests(contests)) {
        cerr << "Error: Duplicate contests found!" << endl;
        return 1;
    }

    // 合并从属信息
    for (auto& [key, player] : players) {
        mergeAffiliations(player.affiliations);
    }
    for (auto& [key, teacher] : teachers) {
        mergeAffiliations(teacher.affiliations);
    }

    // 按总分排序选手
    vector<pair<string, Player>> sortedPlayers(players.begin(), players.end());
    sort(sortedPlayers.begin(), sortedPlayers.end(), [](const auto& a, const auto& b) {
        return a.second.score > b.second.score;
    });

    // 设置排名
    for (size_t i = 0; i < sortedPlayers.size(); ++i) {
        sortedPlayers[i].second.rank = i + 1;
    }

    // 生成选手 Markdown 文件
    for (const auto& [key, player] : sortedPlayers) {
        generatePlayerMarkdown(player, contests, teachers);
    }

    // 生成比赛 Markdown 文件
    for (const auto& [key, contest] : contests) {
        generateContestMarkdown(contest, teachers);
    }

    // 生成老师 Markdown 文件
    for (const auto& [key, teacher] : teachers) {
        generateTeacherMarkdown(teacher);
    }

    // 显示选手、比赛和老师的数量
    cout << "选手数量: " << players.size() << endl;
    cout << "比赛数量: " << contests.size() << endl;
    cout << "老师数量: " << teachers.size() << endl;

    return 0;
}