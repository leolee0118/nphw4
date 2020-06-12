#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/Object.h>
#include <fstream>
#include <sys/stat.h>
#include <nlohmann/json.hpp>
#include <errno.h>
#include <streambuf>

#define bufsize 1024

using namespace std;
using namespace Aws;
using namespace Aws::S3;
using namespace Aws::S3::Model;
using namespace nlohmann;

string prefix = "0616220-";
string postPrefix = "post";
string mailPrefix = "mail";

string postDir = "post/";
string mailDir = "mail/";
json response;

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

user_data client_user;

// functions
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

string newlineForm(string content) {
    return substitute(content, "<br>", "\n");
}

void legalBucketName(string &bucketName) {
	string postfix = "-00";
	for (int i = 0; i < bucketName.size(); i++) {
		if ('A' <= bucketName[i] && bucketName[i] <= 'Z') {
			bucketName[i] = bucketName[i] + 32; // to lowercase
			postfix[1] = '1';
		} else if (bucketName[i] == '_') {
			bucketName[i] = '-';
			postfix[2] = '1';
		}
	}
}

// aws
inline bool ifFileExist(const string& name)
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

bool createBucket(const String &bucketName, const BucketLocationConstraint &region = BucketLocationConstraint::us_east_1) {
    // Set up the request

    CreateBucketRequest request;
    request.SetBucket(bucketName);

    // Is the region other than us-east-1 (N. Virginia)?
    if (region != BucketLocationConstraint::us_east_1)
    {
        // Specify the region as a location constraint
        CreateBucketConfiguration bucket_config;
        bucket_config.SetLocationConstraint(region);
        request.SetCreateBucketConfiguration(bucket_config);
    }

    // Create the bucket
    S3Client s3_client;
    auto outcome = s3_client.CreateBucket(request);
    if (!outcome.IsSuccess())
    {
        auto err = outcome.GetError();
        cout << "ERROR: CreateBucket: " << 
            err.GetExceptionName() << ": " << err.GetMessage() << std::endl;
        return false;
    }
    return true;
}

bool putObject(const String& bucketName, const String& objectName, const string& fileName, const String& region = "") {
    // Verify file_name exists
    if (!ifFileExist(fileName)) {
        cout << "ERROR: NoSuchFile: The specified file does not exist\n";
        return false;
    }

    // If region is specified, use it
    Client::ClientConfiguration clientConfig;
    if (!region.empty())
        clientConfig.region = region;

    // Set up request
    // snippet-start:[s3.cpp.put_object.code]
    S3Client s3_client(clientConfig);
    PutObjectRequest objectRequest;

    objectRequest.SetBucket(bucketName);
    objectRequest.SetKey(objectName);
    const shared_ptr<IOStream> inputData = MakeShared<FStream>("SampleAllocationTag", fileName.c_str(), ios_base::in | ios_base::binary);
    objectRequest.SetBody(inputData);

    // Put the object
    auto put_object_outcome = s3_client.PutObject(objectRequest);
    if (!put_object_outcome.IsSuccess()) {
        auto error = put_object_outcome.GetError();
        cout << "ERROR: " << error.GetExceptionName() << ": " << error.GetMessage() << '\n';
        return false;
    }
    return true;
    // snippet-end:[s3.cpp.put_object.code]
}

bool getObject(const String& bucketName, const String& objectName, const string& fileName) {
	S3Client s3_client;
	GetObjectRequest objectRequest;

	objectRequest.SetBucket(bucketName);
	objectRequest.SetKey(objectName);
	auto get_object_outcome = s3_client.GetObject(objectRequest);
	if (get_object_outcome.IsSuccess()) {
		// Get an Aws::IOStream reference to the retrieved file
		auto &retrievedFile = get_object_outcome.GetResultWithOwnership().GetBody();

		// Alternatively, read the object's contents and write to a file
		ofstream outputFile(fileName, ios::binary);
		outputFile << retrievedFile.rdbuf();    
		return true;
	} else {
		auto error = get_object_outcome.GetError();
		std::cout << "ERROR: " << error.GetExceptionName() << ": " 
			<< error.GetMessage() << std::endl;
		return false;
	}
}

bool deleteObject(const String& bucketName, const String& objectName) {
	// snippet-start:[s3.cpp.delete_object.code]
	S3Client s3_client;

	DeleteObjectRequest objectRequest;
	objectRequest.WithBucket(bucketName).WithKey(objectName);

	auto delete_object_outcome = s3_client.DeleteObject(objectRequest);

	if (delete_object_outcome.IsSuccess()) {
		return true;
	} else {
		cout << "DeleteObject error: " <<
		delete_object_outcome.GetError().GetExceptionName() << " " <<
		delete_object_outcome.GetError().GetMessage() << '\n';
		return false;
	}
}

// handlers
bool register_handler(string name) {
	string uid = response["uid"];
	string bucketName = prefix + uid + name;
	legalBucketName(bucketName);
	return createBucket(bucketName.c_str());
}

bool login_handler(string name) {
	client_user.init();
	client_user.name = name;
	client_user.id = response["uid"];
}

bool createPost_handler(string title, string content) {
	string pid = response["pid"];
	string objectName = postPrefix + pid + title;
	string bucketName = prefix + to_string(client_user.id) + client_user.name;
	legalBucketName(bucketName);
	
	content = content + "\n--\n";

	string fileName = postDir + objectName;
	fstream file;
	file.open(fileName, ios::out);
	file << content;
	file.close();

	return putObject(bucketName.c_str(), objectName.c_str(), fileName);
}

bool updatePost_handler(string pid, string category, string text) {
	string title = response["postTitle"];
	string objectName = postPrefix + pid + title;
	string bucketName = prefix + to_string(client_user.id) + client_user.name;
	legalBucketName(bucketName);
	string fileName = postDir + objectName;
	if (category == "--title") {
		string newObjectName = postPrefix + pid + text;
		string newFileName = postDir + newObjectName;
		if (rename(fileName.c_str(), newFileName.c_str()) != 0) {
			cout << "Error: " << strerror(errno) << '\n';
			return false;
		}
		if (!deleteObject(bucketName.c_str(), objectName.c_str())) {
			return false;
		}
		return putObject(bucketName.c_str(), newObjectName.c_str(), newFileName);
	} else if (category == "--content") {
		ifstream reader(fileName);
		string content((istreambuf_iterator<char>(reader)), istreambuf_iterator<char>());
		size_t found = content.find("\n--\n");
		content.replace(0, found, text);

		fstream file;
		file.open(fileName, ios::out);
		file << content;
		file.close();

		return putObject(bucketName.c_str(), objectName.c_str(), fileName);
	}
}

bool comment_handler(string pid, string comment) {
	string authorId = response["authorId"];
	string author = response["postAuthor"];
	string title = response["postTitle"];
	string completeComment = client_user.name + ":" + comment + "\n";
	string objectName = postPrefix + pid + title;
	string bucketName = prefix + authorId + author;
	legalBucketName(bucketName);
	string fileName = postDir + objectName;

	fstream file(fileName, fstream::out | fstream::app);
	if (file.is_open()) {
		file << completeComment;
		file.close();
	} else {
		cout << "Failed to open file.\n";
		return false;
	}

	return putObject(bucketName.c_str(), objectName.c_str(), fileName);
}

bool deletePost_handler(string pid) {
	string title = response["postTitle"];
	string objectName = postPrefix + pid + title;
	string bucketName = prefix + to_string(client_user.id) + client_user.name;
	legalBucketName(bucketName);
	string fileName = postDir + objectName;
	
	if (remove(fileName.c_str()) != 0) {
		cout << "Error: " << strerror(errno) << '\n';
		return false;
	}
	deleteObject(bucketName.c_str(), objectName.c_str());
	return true;
}

bool read_handler(string pid) {
	string authorId = response["authorId"];
	string title = response["postTitle"];
	string author = response["postAuthor"];
	string date = response["postDate"];
	string objectName = postPrefix + pid + title;
	string bucketName = prefix + authorId + author;
	legalBucketName(bucketName);
	string fileName = postDir + objectName;

	ifstream reader(fileName);
	string post((istreambuf_iterator<char>(reader)), istreambuf_iterator<char>());
	size_t found = post.find("\n--\n");
	string brcontent = post.substr(0, found);
	string content = newlineForm(brcontent);
	string comment = post.substr(found, post.size() - brcontent.size());

	cout << "Author\t:" << author << '\n';
	cout << "Title\t:" << title << '\n';
	cout << "Date\t:" << date << "\n--\n"; 
	cout << content << comment;
}

bool mailto_handler(string content) {
	string mid = response["mid"];
	string subject = response["mailSubject"];
	string receiver = response["mailReceiver"];
	string receiverId = response["receiverId"];
	string objectName = mailPrefix + mid + subject;
	string bucketName = prefix + receiverId + receiver;
	legalBucketName(bucketName);
	string fileName = mailDir + objectName;

	content = content + "\n";

	fstream file;
	file.open(fileName, ios::out);
	file << content;
	file.close();

	return putObject(bucketName.c_str(), objectName.c_str(), fileName);
}

bool retrMail_handler() {
	string mid = response["mid"];
	string subject = response["mailSubject"];
	string sender = response["mailSender"];
	string date = response["mailDate"];
	string objectName = mailPrefix + mid + subject;
	string bucketName = prefix + to_string(client_user.id) + client_user.name;
	legalBucketName(bucketName);
	string fileName = mailDir + objectName;

	ifstream reader(fileName);
	string mail((istreambuf_iterator<char>(reader)), istreambuf_iterator<char>());
	string content = newlineForm(mail);
	
	cout << "Subject\t:" << subject << '\n';
	cout << "From\t:" << sender << '\n';
	cout << "Date\t:" << date << "\n--\n"; 
	cout << content;
}

bool deleteMail_handler() {
	string mid = response["mid"];
	string subject = response["mailSubject"];
	string objectName = mailPrefix + mid + subject;
	string bucketName = prefix + to_string(client_user.id) + client_user.name;
	legalBucketName(bucketName);
	string fileName = mailDir + objectName;

	if (remove(fileName.c_str()) != 0) {
		cout << "Error: " << strerror(errno) << '\n';
		return false;
	}
	return deleteObject(bucketName.c_str(), objectName.c_str());
}

void middleware(const int cid, vector<string> &cmd) {
	if (cid == 0) { // register
		register_handler(cmd[1]);
	} else if (cid == 1) { // login (something's wrong)
		login_handler(cmd[1]);
	} else if (cid == 7) { // create-post
		createPost_handler(cmd[3], cmd[5]);
	} else if (cid == 9) { // update-post
		updatePost_handler(cmd[1], cmd[2], cmd[3]);
	} else if (cid == 10) { // comment
		comment_handler(cmd[1], cmd[2]);
	} else if (cid == 11) { // delete-post
		deletePost_handler(cmd[1]);
	} else if (cid == 12) { // read
		read_handler(cmd[1]);
	} else if (cid == 13) { // mail-to
		mailto_handler(cmd[5]);
	} else if (cid == 15) { // retr-mail
		retrMail_handler();
	} else if (cid == 16) { // delete-mail
		deleteMail_handler();
	}
}

int handler(const char *cres) { // 1: exit, 0: otherwise
	// cout << cres << '\n';
	response.clear();
	response = json::parse(cres);
	int cid = response["cid"], rid = response["rid"];
	string res = response["msg"];
	vector<string> cmd = response["cmd"];
	if (cid == 4 && rid == 0) { // exit
		return 1;
	}
	if (rid == 0) {
		middleware(cid, cmd);
	}
	// cout << "hi?\n";
	// cout << response["msg"];
	if (cmd.size() && cmd[0] != ".INIT") {
		cout << res;
	}
	cout << "% ";
	cout.flush();
	return 0;
}

// int main(int argc, char *argv[]) {
// 	if (argc < 3) {
// 		cerr << "No [hostname, port] provided\n";
// 		exit(1);
// 	}

//     int sockfd, portno;
//     struct sockaddr_in serv_addr;
//     struct hostent *server;

// 	sockfd = socket(AF_INET, SOCK_STREAM, 0);
// 	if (sockfd < 0) {
// 		cerr << "failed to open socket\n";
// 		exit(1);
// 	}

//     bzero((char *) &serv_addr, sizeof(serv_addr));
//     portno = atoi(argv[2]);
//     server = gethostbyname(argv[1]);
//     if (server == NULL) {
//         fprintf(stderr,"ERROR, no such host\n");
//         exit(0);
//     }
    
//     serv_addr.sin_family = AF_INET;
//     bcopy((char *)server->h_addr, 
//          (char *)&serv_addr.sin_addr.s_addr,
//          server->h_length);
//     serv_addr.sin_port = htons(portno);

//     if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
// 		cerr << "failed to connect\n";
// 		exit(1);
// 	}

// 	cout << "fd: " << sockfd << '\n';
 
// 	Aws::SDKOptions options;
//    	Aws::InitAPI(options);

// 	int n, i = 0;
// 	char buffer[bufsize];
	
// 	// read welcome message
// 	bzero(buffer, bufsize);
// 	n = read(sockfd, buffer, bufsize - 1);
// 	if (n < 0) {
// 		cerr << "failed to read from socket\n";
// 		exit(1);
// 	}
// 	cout << buffer;

// 	// n = write(sockfd, buffer, strlen(buffer));
// 	// if (n < 0) {
// 	// 	cerr << "real buf failure\n";
// 	// 	exit(1);
// 	// }

// 	while (true) {
// 		// n = write(sockfd, buffer, strlen(buffer));
// 		// if (n < 0) {
// 		// 	cerr << "real buf failure\n";
// 		// 	exit(1);
// 		// }

// 		// // read %
// 		// bzero(buffer, bufsize);
// 		// n = read(sockfd, buffer, bufsize - 1);
// 		// if (n < 0) {
// 		// 	cerr << "failed to read from socket1\n";
// 		// 	exit(1);
// 		// }
// 		// cout << buffer;
// 		cout << "% ";

// 		// request
// 		bzero(buffer, bufsize);
// 		fgets(buffer, bufsize - 1, stdin);
// 		n = write(sockfd, buffer, strlen(buffer));
// 		if (n < 0) {
// 			cerr << "failed to write to socket\n";
// 			exit(1);
// 		}

// 		// response form: <cid>,<resid>,<res>
// 		bzero(buffer, bufsize);
// 		n = read(sockfd, buffer, bufsize - 1);
// 		if (n < 0) {
// 			cerr << "failed to read from socket2\n";
// 			exit(1);
// 		}
// 		int stat = handler(buffer/*, cmd*/);
// 		if (stat == 1) {
// 			break;
// 		}
// 	}

// 	Aws::ShutdownAPI(options);
// }

int main(int argc, char* argv[]){
    if (argc != 3){
        cerr << "No [hostname, port] provided\n";
		exit(1);
    }
 
    //Initialize
    int sockfd, portno;
	struct sockaddr_in serv_addr;
	struct hostent *server;
    char buffer[bufsize];
    
 
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "failed to open socket\n";
		exit(1);
    }

    bzero(&serv_addr, sizeof(serv_addr));
	portno = atoi(argv[2]);
	server = gethostbyname(argv[1]);
	if (server == NULL) {
        cerr << "ERROR, no such host\n";
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
 
    //Connect to server
    if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) != 0) {
        cerr << "failed to connect\n";
		exit(1);
    } else {
        read(sockfd, buffer, sizeof(buffer));
        cout << buffer;
		write(sockfd, ".INIT", 5);
    }

	fd_set rset;
    FD_ZERO(&rset);

	Aws::SDKOptions options;
   	Aws::InitAPI(options);
 
    //Loop
    while(1){
        int maxfd;
        FD_SET(0, &rset);
        FD_SET(sockfd, &rset);
        maxfd = max(0, sockfd) + 1;
        select(maxfd, &rset, NULL, NULL, NULL);
        //socket is readable
        if (FD_ISSET(sockfd, &rset)) {
			bzero(buffer, bufsize);
            if (read(sockfd, buffer, bufsize - 1) > 0) {
				int stat = handler(buffer/*, cmd*/);
				if (stat == 1) {
					break;
				}
			}
        }
        //input is readable
        if (FD_ISSET(0, &rset)) {
            bzero(buffer, bufsize);
			fgets(buffer, bufsize - 1, stdin);
			if (write(sockfd, buffer, strlen(buffer)) < 0) {
				cerr << "failed to write to socket\n";
				exit(1);
			}
        }
    }
    close(sockfd);

	Aws::ShutdownAPI(options);
}
