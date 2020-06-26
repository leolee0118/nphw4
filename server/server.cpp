#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <map>
#include <utility>
#include <nlohmann/json.hpp>

#define bufsize 1024

using namespace std;
using namespace nlohmann;

char database[] = "test.db";

struct user_data {
    int id, status;
    string name, email, password;
    void init() {
        id = -1;
        name = "";
        email = "";
        password = "";
        status = 0;
    }
};

struct boards_data {
    vector<int> ids, uids;
    vector<string> names, usernames;
    void init() {
        ids.clear();
        names.clear();
        uids.clear();
        usernames.clear();
    }
};

struct posts_data {
    vector<int> ids, uids, bids;
    vector<string> titles, authors, dates, boardnames;
    void init() {
        ids.clear();
        titles.clear();
        authors.clear();
        dates.clear();
        uids.clear();
        boardnames.clear();
        bids.clear();
    }
};

struct mails_data {
    vector<int> ids;
    vector<string> senders, receviers, subjects, dates;
    void init() {
        ids.clear();
        senders.clear();
        receviers.clear();
        subjects.clear();
        dates.clear();
    }
};

struct subscriptions_data {
    vector<int> ids, uids;
    vector<string> usernames, targets, targetNames, keywords;
    void init() {
        ids.clear();
        usernames.clear();
        uids.clear();
        targets.clear();
        targetNames.clear();
        keywords.clear();
    }
};

user_data client_user, client_users;
// user_data client_users;
boards_data client_boards;
posts_data client_posts;
mails_data client_mails;
subscriptions_data client_subs;

map <int, user_data> fd_user;
map <string, int> username_fd;
map <string, string> usage;
map <string, int> cmdSize;
map <string, int> cmdId;
vector<map<int, string> > msgs;
json response;

map<int, string> broadcasts;

// callbacks
static int simple_callback(void *data, int argc, char **argv, char **azColName) {
    return 0;
}

static int login_callback(void *data, int argc, char **argv, char **azColName) {
    if (argc) {
        client_user.id = atoi(argv[0]);
        client_user.name = argv[1];
        client_user.email = argv[2];
        client_user.password = argv[3];
        client_user.status = 1;
    }
    return 0;
}

static int listBoard_callback(void *data, int argc, char **argv, char **azColName) {
    if (argc) {
        for (int i = 0; i < argc; ) {
            client_boards.ids.push_back(atoi(argv[i++]));
            client_boards.names.push_back(argv[i++]);
            client_boards.uids.push_back(atoi(argv[i++]));
            client_boards.usernames.push_back(argv[i++]);
        }
    }
    return 0;
}

static int listPost_callback(void *data, int argc, char **argv, char **azColName) {
    if (argc) {
        for (int i = 0; i < argc; ) {
            client_posts.ids.push_back(atoi(argv[i++]));
            client_posts.titles.push_back(argv[i++]);
            client_posts.authors.push_back(argv[i++]);
            client_posts.dates.push_back(argv[i++]);
            client_posts.uids.push_back(atoi(argv[i++]));
            client_posts.boardnames.push_back(argv[i++]);
            client_posts.bids.push_back(atoi(argv[i++]));
        }
        for (int i = 0; i < argc; i++) {
            cout << argv[i] << '\n';
        }
    }
    return 0;
}

static int listUser_callback(void *data, int argc, char **argv, char **azColName) {
    if (argc) {
        client_users.id = atoi(argv[0]);
        client_users.name = argv[1];
        client_users.email = argv[2];
        client_users.password = argv[3];
        client_users.status = -1;
    }
    return 0;
}

static int listMail_callback(void *data, int argc, char **argv, char **azColName) {
    if (argc) {
        for (int i = 0; i < argc; ) {
            client_mails.ids.push_back(atoi(argv[i++]));
            client_mails.senders.push_back(argv[i++]);
            client_mails.receviers.push_back(argv[i++]);
            client_mails.subjects.push_back(argv[i++]);
            client_mails.dates.push_back(argv[i++]);
        }
    }
    return 0;
}

static int listSubscription_callback(void *data, int argc, char **argv, char **azColName) {
    if (argc) {
        for (int i = 0; i < argc; ) {
            client_subs.ids.push_back(atoi(argv[i++]));
            client_subs.usernames.push_back(argv[i++]);
            client_subs.uids.push_back(atoi(argv[i++]));
            client_subs.targets.push_back(argv[i++]);
            client_subs.targetNames.push_back(argv[i++]);
            client_subs.keywords.push_back(argv[i++]);
        }
    }
    return 0;
}

// functions
void writeMsg(int fd, string msg, int cid, int res) {
    response["msg"] = msg;
    response["cid"] = cid;
    response["rid"] = res;
    cout << response.dump() << '\n';
    write(fd, response.dump().c_str(), response.dump().size());
}

void broadcast() {
    map<int, user_data>::iterator it;
    for (it = fd_user.begin(); it != fd_user.end(); it++) {
        if (it->second.status == 0) {
            continue;
        }
        int fd = it->first;
        string msg = broadcasts[fd];
        cout << "fd: " << fd << " msg: " << msg << '\n';
        if (msg != "") {
            writeMsg(fd, msg, -1, -1);
        }
    }
}

void mapping(int fd, bool get0_put1) {
    if (get0_put1 == 0) {
        client_user = fd_user[fd];
    } else {
        fd_user[fd] = client_user;
        username_fd[client_user.name] = fd;
    }
}

void subscribe_usage(vector<string> &cmd) {
    msgs[17][-1] = "Usage: subscribe --board/--author <boardname>/<authorname> --keyword <keyword>\n";
    msgs[18][-1] = "Usage: unsubscribe --board/--author <boardname>/<authorname>\n";
    if (cmd.size() >= 1) {
        if (cmd[1] == "--board") {
            msgs[17][-1] = "[Invalid option] usage: subscribe --board <boardname> --keyword <keyword>\n";
            msgs[18][-1] = "Usage: unsubscribe --board <boardname>\n";
        } else if (cmd[1] == "--author") {
            msgs[17][-1] = "[Invalid option] usage: subscribe --author <authorname> --keyword <keyword>\n";
            msgs[18][-1] = "Usage: unsubscribe --author <authorname>\n";
        }
    }
}

bool check_email(string email) {
    bool flag = false;
    for (int i = 0; i < email.size(); i++) {
        if (email[i] == '@') {
            flag = true;
        }
    }
    return flag;
}

void getTargetText(const char *req, vector<string> &cmd, string targeta, string targetb) {
    stringstream ss(req);
    string texta, textb, s;
    bool isa = false, isb = false;

    while (ss >> s) {
        if (s == targeta) {
            isa = true;
            if (isb) {
                if (textb.size()) {
                    textb = textb.substr(0, textb.size() - 1);
                    cmd.push_back(textb);
                }
                textb = "";
                isb = false;
            }
            cmd.push_back(s);
            continue;
        } else if (s == targetb) {
            isb = true;
            if (isa) {
                if (texta.size()) {
                    texta = texta.substr(0, texta.size() - 1);
                    cmd.push_back(texta);
                }
                texta = "";
                isa = false;
            }
            cmd.push_back(s);
            continue;
        }
        if (isa) {
            texta = texta + s + ' ';
            continue;
        }
        if (isb) {
            textb = textb + s + ' ';
            continue;
        }
        cmd.push_back(s);
    }
    if (isa) {
        if (texta.size()) {
            texta = texta.substr(0, texta.size() - 1);
            cmd.push_back(texta);
        }
        texta = "";
        isa = false;
    }
    if (isb) {
        if (textb.size()) {
            textb = textb.substr(0, textb.size() - 1);
            cmd.push_back(textb);
        }
        textb = "";
        isb = false;
    }
}

void commentForm(const char *req, vector<string> &cmd) {
    stringstream ss(req);
    string s, sreq(req);
    int i = 0, len = 0;

    while (ss >> s) {
        cmd.push_back(s);
        len += s.size();
        len++;
        i++;
        if (i == 2) {
            break;
        }
    }
    s = sreq.substr(len, sreq.size() - len - 1);
    cmd.push_back(s);
}

bool validId(string id) {
    bool flag = true;
    for (int i = 0; i < id.size(); i++) {
        if (!('0' <= id[i] && id[i] <= '9')) {
            flag = false;
            break;
        }
    }
    return flag;
}

string simpleDateForm(string date) {
    string m = date.substr(5, 2), d = date.substr(8, 2);
    return m + '/' + d;
}

string substitute(string content, string target, string subs) {
    size_t found = -1;
    while (true) {
        found = content.find(target, found + subs.size());
        if (found != string::npos) {
            content.replace(found, target.size(), subs);
        } else {
            break;
        }
    }
    return content;
}

string fixSingleQuote(string content) {
    return substitute(content, "'", "''");
}

// command handlers
int register_handler(string name, string email, string password) {
    bool email_flag = check_email(email); // lost @
    if (!email_flag) {
        return 3; // wrong email form
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "insert into users (name, email, password, status) values ('" + 
                    name + "', '" + email + "', '" + password + "', 0);";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);
    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        if (sqlStat == SQLITE_CONSTRAINT) {
            return 2; // violates unique constraint
        }
        return 1; // other problems
    }
    cout << "Operation done successfully\n";

    response["uid"] = to_string(sqlite3_last_insert_rowid(db));

    sqlite3_close(db);

    return 0; // success
}

int login_handler(string name, string password) {
    msgs[1][0] = "Welcome, " + name + ".\n";

    if (client_user.status == 1) { // Please logout first
        return 2;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "select * from users where name='" + name + "' and password='" + password + "';";
    cout << sql << '\n';
    sqlStat = sqlite3_exec(db, sql.c_str(), login_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_user.status != 1) {
            return 3;
        }
        cout << "Selection done successfully\n";
    }

    response["uid"] = client_user.id;

    sql = "update users set status=status+1 where name='" + name + "';";
    cout << sql << '\n';
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);
    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    }

    sqlite3_close(db);

    return 0;
}

int logout_handler() {
    msgs[2][0] = "Bye, " + client_user.name + ".\n";

    if (client_user.status == 0) { // Please login first
        return 2;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";
    
    string sql = "update users set status=status-1 where name='" + client_user.name + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    client_user.init();

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_user.status != 0) {
            return 1;
        }
        cout << "Selection done successfully\n";
    }

    sqlite3_close(db);

    return 0;
}

int whoami_handler() {
    msgs[3][0] = client_user.name + '\n';

    if (client_user.status != 0) {
        return 0;
    } else {
        return 1;
    }
}

int exit_handler() {
    if (client_user.status == 0) {
        return 0;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "update users set status=status-1 where name='" + client_user.name + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    client_user.init();

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_user.status != 0) {
            return 1;
        }
        cout << "Selection done successfully\n";
    }

    sqlite3_close(db);

    return 0;
}

int createBoard_handler(string name) {
    if (client_user.status == 0) {
        return 3;
    }

    name = fixSingleQuote(name);

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "insert into boards (name, uid, username) values ('" + name + "', " + 
                    to_string(client_user.id) + ", '" + client_user.name + "');";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        if (sqlStat == SQLITE_CONSTRAINT) {
            return 2; // violates unique constraint
        }
        return 1;
    }
    cout << "Operation done successfully.\n";

    sqlite3_close(db);

    return 0;
}

int listBoard_handler(string key) {
    client_boards.init();
    if (key[0] != '#' || key[1] != '#') {
        return -1;
    }

    key = fixSingleQuote(key.substr(2, key.size() - 2));

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";
    string sql = "select * from boards where instr(name, '" + key + "') != 0;";
    sqlStat = sqlite3_exec(db, sql.c_str(), listBoard_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        msgs[6][0] = "Index\tName\tModerator\n";
        for (int i = 0; i < client_boards.ids.size(); i++) {
            msgs[6][0] = msgs[6][0] + to_string(client_boards.ids[i]) + '\t' + 
                        client_boards.names[i] + '\t' + client_boards.usernames[i] + '\n';
        }
        cout << "Selection done successfully\n";
    }

    sqlite3_close(db);

    return 0;
}

int createPost_handler(vector<string> cmd) {
    client_posts.init();
    client_boards.init();
    client_subs.init();
    if (client_user.status == 0) {
        return 3;
    }
    if (cmd[2] != "--title" || cmd[4] != "--content") {
        return -1;
    }
    string title = fixSingleQuote(cmd[3].substr(0, cmd[3].size()));
    string boardname = fixSingleQuote(cmd[1]);

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";
    string sql = "select * from boards where name = '" + boardname + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listBoard_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_boards.ids.size() == 0) {
            return 2;
        }
        cout << "Selection done successfully\n";
    }

    sql = "insert into posts (title, author, uid, boardname, bid) values ('" 
            + title + "', '" + client_user.name + "', " 
            + to_string(client_user.id) + ", '" + fixSingleQuote(client_boards.names[0]) + "', " 
            + to_string(client_boards.ids[0]) + ");";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    }
    cout << "Operation done successfully.\n";

    response["pid"] = to_string(sqlite3_last_insert_rowid(db));

    sql = "select * from subscriptions where (target='board' and targetName='" + 
            client_boards.names[0] + "') or (target='author' and targetName='" + client_user.name + "');";
    sqlStat = sqlite3_exec(db, sql.c_str(), listSubscription_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        cout << "Selection done successfully\n";
    }

    for (int i = 0; i < client_subs.ids.size(); i++) {
        if (title.find(client_subs.keywords[i]) != string::npos) {
            string msg = "*[" + client_boards.names[0] + "] " + title + " - by " + client_user.name + "*\n";
            cout << msg;
            cout << "Name: " << client_subs.usernames[i] << "fd: " << username_fd[client_subs.usernames[i]] << '\n'; 
            broadcasts[username_fd[client_subs.usernames[i]]] = msg;
        }
    }
    sqlite3_close(db);

    return 0;
}

int listPost_handler(string boardname, string key) {
    client_boards.init();
    client_posts.init();
    if (key[0] != '#' || key[1] != '#') {
        return -1;
    }

    boardname = fixSingleQuote(boardname);
    key = key.substr(2, key.size() - 2);

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";
    string sql = "select * from boards where name = '" + boardname + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listBoard_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_boards.ids.size() == 0) {
            return 2;
        }
        cout << "Selection done successfully\n";
    }

    sql = "select * from posts where boardname = '" + boardname + "' and instr(title, '" + key + "') != 0";
    sqlStat = sqlite3_exec(db, sql.c_str(), listPost_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        msgs[8][0] = "ID\tTitle\tAuthor\tDate\n";
        for (int i = 0; i < client_posts.ids.size(); i++) {
            msgs[8][0] = msgs[8][0] + to_string(client_posts.ids[i]) + '\t' + 
                        client_posts.titles[i] + '\t' + client_posts.authors[i] + '\t' +
                        simpleDateForm(client_posts.dates[i]) + '\n';
        }
        cout << "Selection done successfully\n";
    }

    sqlite3_close(db);

    return 0;
}

int updatePost_handler(vector<string> cmd) {
    client_posts.init();
    if (client_user.status == 0) {
        return 2;
    }
    if ((cmd[2] != "--title" && cmd[2] != "--content") || !validId(cmd[1])) {
        return -1;
    }
    string pid = cmd[1];
    string category = cmd[2].substr(2, cmd[2].size() - 2);
    string text = fixSingleQuote(cmd[3].substr(0, cmd[3].size()));

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";
    
    string sql = "select * from posts where pid = " + pid + ";";
    sqlStat = sqlite3_exec(db, sql.c_str(), listPost_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_posts.ids.size() == 0) {
            return 3;
        }
        cout << "Selection done successfully.\n";
    }

    if (client_posts.uids[0] != client_user.id) {
        return 4;
    }

    response["postTitle"] = client_posts.titles[0];

    if (category == "content") {
        return 0;
    }

    sql = "update posts set " + category + " = '" + text + "' where pid=" + pid + ";";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    }
    cout << "Operation done successfully.\n";

    sqlite3_close(db);

    return 0;
}

int comment_handler(string pid) {
    client_posts.init();
    if (client_user.status == 0) {
        return 2;
    }
    if (!validId(pid)) {
        return -1;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "select * from posts where pid=" + pid + ";";
    sqlStat = sqlite3_exec(db, sql.c_str(), listPost_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_posts.ids.size() == 0) {
            return 3;
        }
        cout << "Selection done successfully.\n";
    }

    response["authorId"] = to_string(client_posts.uids[0]);
    response["postAuthor"] = client_posts.authors[0];
    response["postTitle"] = client_posts.titles[0];

    sqlite3_close(db);

    return 0;
}

int deletePost_handler(string pid) {
    client_posts.init();
    if (client_user.status == 0) {
        return 2;
    }
    if (!validId(pid)) {
        return -1;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "select * from posts where pid=" + pid + ";";
    sqlStat = sqlite3_exec(db, sql.c_str(), listPost_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_posts.ids.size() == 0) {
            return 3;
        }
        cout << "Selection done successfully.\n";
    }

    if (client_user.id != client_posts.uids[0]) {
        return 4;
    }

    response["postTitle"] = client_posts.titles[0];

    sql = "delete from posts where pid=" + pid + ";";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    }

    sqlite3_close(db);

    return 0;
}

int read_handler(string pid) {
    client_posts.init();
    // client_comments.init();
    if (!validId(pid)) {
        return -1;
    }
    
    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "select * from posts where pid=" + pid + ";";
    sqlStat = sqlite3_exec(db, sql.c_str(), listPost_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_posts.ids.size() == 0) {
            return 2;
        }
        cout << "Selection done successfully.\n";
    }

    response["authorId"] = to_string(client_posts.uids[0]);
    response["postAuthor"] = client_posts.authors[0];
    response["postTitle"] = client_posts.titles[0];
    response["postDate"] = client_posts.dates[0];

    sqlite3_close(db);

    return 0;
}

int mailto_handler(string receiver, string subject) {
    msgs[13][3] = receiver + " does not exist.\n";
    client_users.init();
    if (client_user.status == 0) {
        return 2;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "select * from users where name='" + receiver + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listUser_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_users.id == -1) {
            return 3;
        }
        cout << "Selection done successfully.\n";
    }

    response["receiverId"] = to_string(client_users.id);

    sql = "insert into mails (sender, receiver, subject) values ('" + 
                    client_user.name + "', '" + receiver + "', '" + subject + "');";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);
    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1; // other problems
    }
    cout << "Operation done successfully\n";

    response["mid"] = to_string(sqlite3_last_insert_rowid(db));
    response["mailSubject"] = subject;
    response["mailReceiver"] = receiver;

    sqlite3_close(db);

    return 0;
}

int listMail_handler() {
    msgs[14][0] = "ID\tSubject\tFrom\tDate\n";
    client_mails.init();
    if (client_user.status == 0) {
        return 2;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
        return 1;
    }
    cout << "Opened database successfully\n";

    string sql = "select * from mails where receiver='" + client_user.name + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listMail_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    } else {
        for (int i = 0; i < client_mails.ids.size(); i++) {
            msgs[14][0] = msgs[14][0] + to_string(i + 1) + "\t" + client_mails.subjects[i] + "\t" 
                        + client_mails.senders[i] + "\t" + simpleDateForm(client_mails.dates[i]) + "\n";
        }
        cout << "Selection done successfully.\n";
        sqlite3_close(db);
        return 0;
    }

}

int retrMail_handler(string id) {
    int iid = stoi(id);
    client_mails.init();
    if (client_user.status == 0) {
        return 2;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
        return 1;
    }
    cout << "Opened database successfully\n";

    string sql = "select * from mails where receiver='" + client_user.name + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listMail_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    } else {
        if (iid > client_mails.ids.size()) {
            return 3;
        }
        response["mid"] = to_string(client_mails.ids[iid - 1]);
        response["mailSubject"] = client_mails.subjects[iid - 1];
        response["mailSender"] = client_mails.senders[iid - 1];
        response["mailDate"] = client_mails.dates[iid - 1];
        cout << "Selection done successfully.\n";
        sqlite3_close(db);
        return 0;
    }
}

int deleteMail_handler(string id) {
    int iid = stoi(id);
    client_mails.init();
    if (client_user.status == 0) {
        return 2;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
        return 1;
    }
    cout << "Opened database successfully\n";

    string sql = "select * from mails where receiver='" + client_user.name + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listMail_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (iid > client_mails.ids.size()) {
            return 3;
        }
        response["mid"] = to_string(client_mails.ids[iid - 1]);
        response["mailSubject"] = client_mails.subjects[iid - 1];
        cout << "Selection done successfully.\n";
    }

    sql = "delete from mails where mid=" + to_string(client_mails.ids[iid - 1]) + ";";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    }

    sqlite3_close(db);
}

int subscribe_handler(string target, string targetName, string keyword) {
    if (client_user.status == 0) {
        return 2;
    }
    if (target != "--board" && target != "--author") {
        return -1;
    }
    
    target = target.substr(2, target.size() - 2);

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "insert into subscriptions (username, uid, target, targetname, keyword) values ('" + 
                    client_user.name + "', '" + to_string(client_user.id) + "', '" + 
                    target + "', '" + targetName + "', '" + keyword + "');";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);
    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        if (sqlStat == SQLITE_CONSTRAINT) {
            return 4; // violates unique constraint
        }
        return 1; // other problems
    }
    cout << "Operation done successfully\n";

    sqlite3_close(db);

    return 0;
}

int unsubscribe_handler(string target, string targetName) {
    msgs[18][3] = "You haven't subscribed " + targetName + ".\n";
    client_subs.init();
    if (client_user.status == 0) {
        return 2;
    }
    if (target != "--board" && target != "--author") {
        return -1;
    }

    target = target.substr(2, target.size() - 2);

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "select * from subscriptions where username='" + client_user.name + 
                "' and target='" + target + "' and targetname='" + targetName + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listSubscription_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    } else {
        if (client_subs.ids.size() == 0) {
            return 3;
        }
    }

    sql = "delete from subscriptions where username='" + client_user.name + 
                "' and target='" + target + "' and targetname='" + targetName + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), simple_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    }

    sqlite3_close(db);

    return 0;
}

int listSub_handler() {
    client_subs.init();
    if (client_user.status == 0) {
        return 2;
    }

    sqlite3 *db;
    char *errMsg = 0;
    int sqlStat;

    if (sqlite3_open(database, &db)) {
        cerr << "can't open database.\n";
    }
    cout << "Opened database successfully\n";

    string sql = "select * from subscriptions where username='" + client_user.name + "';";
    sqlStat = sqlite3_exec(db, sql.c_str(), listSubscription_callback, 0, &errMsg);

    if (sqlStat != SQLITE_OK) {
        cerr << "sql error: " << errMsg << '\n';
        cerr << "last sql: " << sql << '\n';
        sqlite3_free(errMsg);
        return 1;
    }

    msgs[19][0] = "Type\tName\tKeyword\n";
    for (int i = 0; i < client_subs.ids.size(); i++) {
        msgs[19][0] = msgs[19][0] + client_subs.targets[i] + '\t' + 
                    client_subs.targetNames[i] + '\t' + client_subs.keywords[i] + '\n';
    }

    return 0;
}

int middleware(int cid, vector<string> cmd) {
    cout << cid << ' ' << cmd[0] << '\n';
    int res;
    if (cid == 0) { // register
        res = register_handler(cmd[1], cmd[2], cmd[3]);
    } else if (cid == 1) { // login
        res = login_handler(cmd[1], cmd[2]);
    } else if (cid == 2) { // logout
        res = logout_handler();
    } else if (cid == 3) { // whoami
        res = whoami_handler();
    } else if (cid == 4) { // exit
        res = exit_handler();
    } else if (cid == 5) {
        res = createBoard_handler(cmd[1]);
    } else if (cid == 6) {
        res = listBoard_handler(cmd[1]);
    } else if (cid == 7) {
        res = createPost_handler(cmd);
    } else if (cid == 8) {
        res = listPost_handler(cmd[1], cmd[2]);
    } else if (cid == 9) {
        res = updatePost_handler(cmd);
    } else if (cid == 10) {
        res = comment_handler(cmd[1]);
    } else if (cid == 11) {
        res = deletePost_handler(cmd[1]);
    } else if (cid == 12) {
        res = read_handler(cmd[1]);
    } else if (cid == 13) {
        res = mailto_handler(cmd[1], cmd[3]);
    } else if (cid == 14) {
        res = listMail_handler();
    } else if (cid == 15) {
        res = retrMail_handler(cmd[1]);
    } else if (cid == 16) {
        res = deleteMail_handler(cmd[1]);
    } else if (cid == 17) {
        res = subscribe_handler(cmd[1], cmd[2], cmd[4]);
    } else if (cid == 18) {
        res = unsubscribe_handler(cmd[1], cmd[2]);
    } else if (cid == 19) {
        res = listSub_handler();
    }
    return res;
}

int handler(const char *req, int fd) { // 1: exit, 0: otherwise
    stringstream ss(req);
    vector<string> cmd;
    string s, sreq(req);
    
    if (sreq.substr(0, 11) == "create-post" || sreq.substr(0, 11) == "update-post") {
        getTargetText(req, cmd, "--title", "--content");
    } else if (sreq.substr(0, 7) == "comment") {
        commentForm(req, cmd);
    } else if (sreq.substr(0, 7) == "mail-to") {  
        getTargetText(req, cmd, "--subject", "--content");
    } else {
        while (ss >> s) {
            cmd.push_back(s);
        }
    }
    for (int i = 0; i < cmd.size(); i++) {
        cout << cmd[i] << '\n';
    }

    response.clear();
    response["cmd"] = cmd;

    if (cmd.size() == 0) { // if char contains only newline
        writeMsg(fd, "", -1, -1);
        return 0;
    }
    if (cmdSize.find(cmd[0]) == cmdSize.end()) {
        string notFound = cmd[0] + ": command not found\n";
        writeMsg(fd, notFound, -1, -1);
        return 0;
    }
    if ((cmd[0] == "list-board" || cmd[0] == "list-post") && cmd.size() == cmdSize[cmd[0]] - 1) {
        cmd.push_back("##");
    }

    response["cmd"] = cmd;
    int cid = cmdId[cmd[0]];

    if (cmd[0] == "subscribe" || cmd[0] == "unsubscribe") {
        subscribe_usage(cmd);
    }

    if (cmd.size() != cmdSize[cmd[0]]) {
        cout << "usage error\n";
        writeMsg(fd, msgs[cid][-1], cid, -1);
        return 0;
    }

    broadcasts.clear();
    mapping(fd, 0);
    int res = middleware(cid, cmd);
    mapping(fd, 1);
    if (cmd[0] == "exit" && res == 0) {
        writeMsg(fd, ".EXIT", cid, res);
        return 1;
    }
    writeMsg(fd, msgs[cid][res], cid, res);
    broadcast();
    return 0;
}

void build() {
    cmdSize["register"] = 4;
    cmdSize["login"] = 3;
    cmdSize["logout"] = 1;
    cmdSize["whoami"] = 1;
    cmdSize["exit"] = 1;
    cmdSize["create-board"] = 2;
    cmdSize["list-board"] = 2;
    cmdSize["create-post"] = 6;
    cmdSize["list-post"] = 3;
    cmdSize["update-post"] = 4;
    cmdSize["comment"] = 3;
    cmdSize["delete-post"] = 2;
    cmdSize["read"] = 2;
    cmdSize["mail-to"] = 6;
    cmdSize["list-mail"] = 1;
    cmdSize["retr-mail"] = 2;
    cmdSize["delete-mail"] = 2;
    cmdSize["subscribe"] = 5;
    cmdSize["unsubscribe"] = 3;
    cmdSize["list-sub"] = 1;

    cmdId["register"] = 0;
    cmdId["login"] =  1;
    cmdId["logout"] = 2;
    cmdId["whoami"] = 3;
    cmdId["exit"] = 4;
    cmdId["create-board"] = 5;
    cmdId["list-board"] = 6;
    cmdId["create-post"] = 7;
    cmdId["list-post"] = 8;
    cmdId["update-post"] = 9;
    cmdId["comment"] = 10;
    cmdId["delete-post"] = 11;
    cmdId["read"] = 12;
    cmdId["mail-to"] = 13;
    cmdId["list-mail"] = 14;
    cmdId["retr-mail"] = 15;
    cmdId["delete-mail"] = 16;
    cmdId["subscribe"] = 17;
    cmdId["unsubscribe"] = 18;
    cmdId["list-sub"] = 19;

    msgs.resize(cmdId.size());

    msgs[0][-1] = "Usage: register <username> <email> <password>\n";
    msgs[0][0] = "Register successfully.\n";
    msgs[0][1] = "Some problems\n";
    msgs[0][2] = "Username is already used.\n";
    msgs[0][3] = "Wrong email form\n";

    msgs[1][-1] = "Usage: login <username> <password>\n";
    msgs[1][1] = "Some problems\n";
    msgs[1][2] = "Please logout first.\n";
    msgs[1][3] = "Login failed.\n";

    msgs[2][-1] = "Usage: logout\n";
    msgs[2][1] = "Some problems.\n";
    msgs[2][2] = "Please login first.\n";

    msgs[3][-1] = "Usage: whoami\n";
    msgs[3][1] = "Please login first.\n";

    msgs[4][-1] = "Usage: exit\n";
    msgs[4][1] = "Some problems\n";

    msgs[5][-1] = "Usage: create-board <name>\n";
    msgs[5][0] = "Create board successfully.\n";
    msgs[5][1] = "Some problems.\n";
    msgs[5][2] = "Board already exist.\n";
    msgs[5][3] = "Please login first.\n";

    msgs[6][-1] = "Usage: list-board ##<key>\n";
    // msgs[6][0] = "Index\tName\tModerator\n";
    msgs[6][1] = "Some problems.\n";
    
    msgs[7][-1] = "Usage: create-post <board-name> --title <title> --content <content>\n";
    msgs[7][0] = "Create post successfully.\n";
    msgs[7][1] = "Some problems.\n";
    msgs[7][2] = "Board does not exist.\n";
    msgs[7][3] = "Please login first.\n";

    msgs[8][-1] = "Usage: list-post <board-name> ##<key>\n";
    // msgs[8][0] = "ID\tTitle\tAuthor\tDate";
    msgs[8][1] = "Some problems.\n";
    msgs[8][2] = "Board does not exist.\n";

    msgs[9][-1] = "Usage: update-post <post-id> --title/content <new>\n";
    msgs[9][0] = "Update successfully.\n";
    msgs[9][1] = "Some problems.\n";
    msgs[9][2] = "Please login first.\n";
    msgs[9][3] = "Post does not exist.\n";
    msgs[9][4] = "Not the post owner.\n";

    msgs[10][-1] = "Usage: comment <post-id> <comment>\n";
    msgs[10][0] = "Comment successfully.\n";
    msgs[10][1] = "Some problems\n";
    msgs[10][2] = "Please login first.\n";
    msgs[10][3] = "Post does not exist.\n";

    msgs[11][-1] = "Usage: delete-post <post-id>\n";
    msgs[11][0] = "Delete successfully.\n";
    msgs[11][1] = "Some problems\n";
    msgs[11][2] = "Please login first.\n";
    msgs[11][3] = "Post does not exist.\n";
    msgs[11][4] = "Not the post owner.\n";

    msgs[12][-1] = "Usage: read <post-id>\n";
    msgs[12][0] = "";
    msgs[12][1] = "Some problems.\n";
    msgs[12][2] = "Post does not exist.\n";

    msgs[13][-1] = "Usage: mail-to <username> --subject <subject> --content <content>\n";
    msgs[13][0] = "Sent successfully.\n";
    msgs[13][1] = "Some problems.\n";
    msgs[13][2] = "Please login first.\n";

    msgs[14][-1] = "Usage: list-mail\n";
    // msgs[14][0] =
    msgs[14][1] = "Some problems.\n";
    msgs[14][2] = "Please login first.\n"; 

    msgs[15][-1] = "Usage: retr-mail <mail#>\n";
    // msgs[15][0]
    msgs[15][1] = "Some problems.\n";
    msgs[15][2] = "Please login first.\n";
    msgs[15][3] = "No much mail.\n";

    msgs[16][-1] = "Usage: delete-mail <mail#>\n";
    msgs[16][0] = "Mail deleted.\n";
    msgs[16][1] = "Some problems.\n";
    msgs[16][2] = "Please login first.\n";
    msgs[16][3] = "No such mail.\n";

    msgs[17][-1] = "Usage: subscribe --board/--author <boardname>/<authorname> --keyword <keyword>\n";
    msgs[17][0] = "Subscribe successfully.\n";
    msgs[17][1] = "Some problems.\n";
    msgs[17][2] = "Please login first.\n";
    // msgs[17][3] = ""
    msgs[17][4] = "Already subscribed.\n";

    msgs[18][-1] = "Usage: unsubscribe --board/--author <boardname>/<authorname>\n";
    msgs[18][0] = "Unsubscribe successfully.\n";
    msgs[18][1] = "Some problems.\n";
    msgs[18][2] = "Please login first.\n";
    // msgs[18][3] = 

    msgs[19][-1] = "Usage: list-sub\n";
    // msgs[19][0]
    msgs[19][1] = "Some problems.\n";
    msgs[19][2] = "Please login first.\n";
}

int main(int argc, char **argv) {
    if(argc != 2) {
		cerr << "No port provided\n";
		exit(1);
	}
    
    int sockfd, portno; // file descriptor, port number
    struct sockaddr_in server_addr;
    socklen_t len;

    bzero(&server_addr, sizeof(server_addr));
    portno = atoi(argv[1]);

    // set serv_addr info
	server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(portno);
    len = sizeof(struct sockaddr_in);

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Socket Error\n");
        exit(1);
    }

    int flag = 1;
    /* Set socket option */
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0) {
        printf("setsocket Error\n");
        exit(1);
    }

    /* Bind */
	if (::bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		cerr << "failed to bind\n";
        exit(1);
    }

    /* Listen */
	if (listen(sockfd, 10) == -1) {
        perror("listen Error");
        exit(3);
    }

    fd_set master; // master file descriptor set
    FD_ZERO(&master); // initialize master

    // add sockfd to master
    FD_SET(sockfd, &master);

    // track the max fd
    int fdmax = sockfd;

    char buffer[bufsize];

    client_user.init();
    build();

    while(true) {
        fd_set read_fds;
        read_fds = master;

        if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select Error");
            exit(4);
        }

        for(int fd = 0; fd <= fdmax; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd == sockfd) {
                    struct sockaddr_in client_addr;
                    // handle new connections
                    int accepted_fd;
                    accepted_fd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
                    write(accepted_fd, "********************************\n** Welcome to the BBS server. **\n********************************\n", 99);
                    // recv(accepted_fd, buffer, sizeof(buffer), 0);
                    // write(accepted_fd, "hi", 2);
                    if (accepted_fd < 0) {
                        cerr << "failed to accept\n";
                    } else {
                        FD_SET(accepted_fd, &master); // add to fd set
                        if (accepted_fd > fdmax) { // track the max fd
                            fdmax = accepted_fd;
                        }
                        cout << "New connection\n";
                    }
                } else {
                    int n;
                    bzero(buffer, sizeof(buffer));
                    // handle requests from clients
                    if ((n = recv(fd, buffer, sizeof(buffer), 0)) <= 0) {
                        // got error or connection closed by client
                        if (n == 0) {
                            client_user.init();
                            mapping(fd, 1);
                            cout << "close by client, fd: " << fd << '\n';
                        } else {
                            cerr << "failed to read from socket\n";
                        }
                        close(fd);
                        FD_CLR(fd, &master);
                    } else {
                        int res = handler(buffer, fd); // handle request
                        if (res == 1) {
                            close(fd);
                            FD_CLR(fd, &master);
                        }
                    }
                }
            }
        }
    }
	return 0;	
}
