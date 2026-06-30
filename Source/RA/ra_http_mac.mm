#include "ra_http_mac.h"

#import <Foundation/Foundation.h>

#include <map>
#include <mutex>

namespace Xm8Ra {

class RaMacHttpClient;

} // namespace Xm8Ra

@interface Xm8RaMacHttpDelegate : NSObject <NSURLSessionDataDelegate>
- (instancetype)initWithOwner:(Xm8Ra::RaMacHttpClient *)owner;
- (void)detachOwner;
@end

namespace Xm8Ra {

struct MacHttpTaskState {
	RaHttpRequest request;
	NSMutableData *data = nil;
	int http_status = 0;
	bool canceled = false;
	bool oversize = false;
	uint32_t redirects = 0;
};

class RaMacHttpClient : public RaHttpClient {
public:
	explicit RaMacHttpClient(const std::string& user_agent);
	~RaMacHttpClient() override;

	void Send(const RaHttpRequest& request) override;
	void Cancel(uint64_t request_id) override;
	void CancelAll() override;
	void DrainCompleted(std::vector<RaHttpResponse> *output) override;

	void DidReceiveResponse(NSURLSessionDataTask *task,
		NSURLResponse *response);
	void DidReceiveData(NSURLSessionDataTask *task, NSData *data);
	void DidComplete(NSURLSessionTask *task, NSError *error);
	void WillRedirect(NSURLSessionTask *task,
		NSHTTPURLResponse *response,
		NSURLRequest *request,
		void (^completionHandler)(NSURLRequest *));

private:
	MacHttpTaskState *FindTaskLocked(NSUInteger task_id);
	void PushCompletedLocked(const MacHttpTaskState& state,
		RaHttpTransportResult result, NSString *content_type,
		NSString *error_message);
	RaHttpTransportResult ClassifyError(const MacHttpTaskState& state,
		NSError *error) const;
	bool RedirectAllowedLocked(MacHttpTaskState *state,
		NSURLRequest *request);
	static NSString *NSStringFromStdString(const std::string& value);

	std::mutex mutex_;
	std::string user_agent_;
	Xm8RaMacHttpDelegate *delegate_ = nil;
	NSURLSession *session_ = nil;
	std::map<NSUInteger, MacHttpTaskState> tasks_;
	std::vector<RaHttpResponse> completed_;
};

RaMacHttpClient::RaMacHttpClient(const std::string& user_agent)
	: user_agent_(user_agent)
{
	delegate_ = [[Xm8RaMacHttpDelegate alloc] initWithOwner:this];
	NSURLSessionConfiguration *configuration =
		[NSURLSessionConfiguration ephemeralSessionConfiguration];
	configuration.HTTPShouldSetCookies = NO;
	configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
	session_ = [[NSURLSession sessionWithConfiguration:configuration
		delegate:delegate_
		delegateQueue:nil] retain];
}

RaMacHttpClient::~RaMacHttpClient()
{
	CancelAll();
	[delegate_ detachOwner];
	[session_ invalidateAndCancel];
	[session_ release];
	[delegate_ release];
}

void RaMacHttpClient::Send(const RaHttpRequest& request)
{
	NSURL *url = [NSURL URLWithString:NSStringFromStdString(request.url)];
	if (url == nil) {
		std::lock_guard<std::mutex> lock(mutex_);
		MacHttpTaskState state;
		state.request = request;
		PushCompletedLocked(state, RaHttpTransportResult::ClientError,
			nil, @"invalid URL");
		return;
	}

	NSMutableURLRequest *url_request =
		[NSMutableURLRequest requestWithURL:url
			cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
			timeoutInterval:request.total_timeout_ms / 1000.0];
	[url_request setHTTPShouldHandleCookies:NO];
	[url_request setValue:NSStringFromStdString(user_agent_)
		forHTTPHeaderField:@"User-Agent"];
	if (request.has_post_data) {
		[url_request setHTTPMethod:@"POST"];
		[url_request setHTTPBody:
			[NSData dataWithBytes:request.post_data.data()
				length:request.post_data.size()]];
		if (!request.content_type.empty()) {
			[url_request setValue:NSStringFromStdString(request.content_type)
				forHTTPHeaderField:@"Content-Type"];
		}
	} else {
		[url_request setHTTPMethod:@"GET"];
	}

	NSURLSessionDataTask *task = [session_ dataTaskWithRequest:url_request];
	{
		std::lock_guard<std::mutex> lock(mutex_);
		MacHttpTaskState state;
		state.request = request;
		state.data = [[NSMutableData alloc] init];
		tasks_[task.taskIdentifier] = state;
	}
	[task resume];
}

void RaMacHttpClient::Cancel(uint64_t request_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto& entry : tasks_) {
		if (entry.second.request.request_id == request_id) {
			entry.second.canceled = true;
			break;
		}
	}

	[session_ getTasksWithCompletionHandler:
		^(NSArray<NSURLSessionDataTask *> *dataTasks,
		  NSArray<NSURLSessionUploadTask *> *,
		  NSArray<NSURLSessionDownloadTask *> *) {
			for (NSURLSessionDataTask *task in dataTasks) {
				std::lock_guard<std::mutex> callback_lock(mutex_);
				auto it = tasks_.find(task.taskIdentifier);
				if (it != tasks_.end() &&
					it->second.request.request_id == request_id) {
					[task cancel];
				}
			}
		}];
}

void RaMacHttpClient::CancelAll()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto& entry : tasks_) {
			entry.second.canceled = true;
		}
	}
	[session_ getTasksWithCompletionHandler:
		^(NSArray<NSURLSessionDataTask *> *dataTasks,
		  NSArray<NSURLSessionUploadTask *> *uploadTasks,
		  NSArray<NSURLSessionDownloadTask *> *downloadTasks) {
			for (NSURLSessionTask *task in dataTasks) {
				[task cancel];
			}
			for (NSURLSessionTask *task in uploadTasks) {
				[task cancel];
			}
			for (NSURLSessionTask *task in downloadTasks) {
				[task cancel];
			}
		}];
}

void RaMacHttpClient::DrainCompleted(std::vector<RaHttpResponse> *output)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (output == nullptr) {
		completed_.clear();
		return;
	}
	output->insert(output->end(), completed_.begin(), completed_.end());
	completed_.clear();
}

void RaMacHttpClient::DidReceiveResponse(NSURLSessionDataTask *task,
	NSURLResponse *response)
{
	std::lock_guard<std::mutex> lock(mutex_);
	MacHttpTaskState *state = FindTaskLocked(task.taskIdentifier);
	if (state == nullptr) {
		return;
	}

	NSHTTPURLResponse *http_response =
		[response isKindOfClass:[NSHTTPURLResponse class]] ?
		(NSHTTPURLResponse *)response : nil;
	if (http_response != nil) {
		state->http_status = (int)[http_response statusCode];
	}
}

void RaMacHttpClient::DidReceiveData(NSURLSessionDataTask *task, NSData *data)
{
	std::lock_guard<std::mutex> lock(mutex_);
	MacHttpTaskState *state = FindTaskLocked(task.taskIdentifier);
	if (state == nullptr || state->data == nil || state->oversize) {
		return;
	}

	if ([state->data length] + [data length] >
		state->request.max_response_bytes) {
		state->oversize = true;
		[task cancel];
		return;
	}
	[state->data appendData:data];
}

void RaMacHttpClient::DidComplete(NSURLSessionTask *task, NSError *error)
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = tasks_.find(task.taskIdentifier);
	if (it == tasks_.end()) {
		return;
	}

	MacHttpTaskState state = it->second;
	tasks_.erase(it);

	NSHTTPURLResponse *http_response =
		[[task response] isKindOfClass:[NSHTTPURLResponse class]] ?
		(NSHTTPURLResponse *)[task response] : nil;
	NSString *content_type = http_response == nil ? nil :
		[http_response.allHeaderFields objectForKey:@"Content-Type"];
	RaHttpTransportResult result = error == nil ?
		RaHttpTransportResult::Success : ClassifyError(state, error);
	PushCompletedLocked(state, result, content_type,
		error == nil ? nil : [error localizedDescription]);
	[state.data release];
}

void RaMacHttpClient::WillRedirect(NSURLSessionTask *task,
	NSHTTPURLResponse *,
	NSURLRequest *request,
	void (^completionHandler)(NSURLRequest *))
{
	std::lock_guard<std::mutex> lock(mutex_);
	MacHttpTaskState *state = FindTaskLocked(task.taskIdentifier);
	if (state == nullptr || !RedirectAllowedLocked(state, request)) {
		completionHandler(nil);
		return;
	}
	state->redirects++;
	completionHandler(request);
}

MacHttpTaskState *RaMacHttpClient::FindTaskLocked(NSUInteger task_id)
{
	auto it = tasks_.find(task_id);
	return it == tasks_.end() ? nullptr : &it->second;
}

void RaMacHttpClient::PushCompletedLocked(const MacHttpTaskState& state,
	RaHttpTransportResult result, NSString *content_type,
	NSString *error_message)
{
	RaHttpResponse response;
	response.request_id = state.request.request_id;
	response.transport_result = result;
	response.http_status = state.http_status;
	if (content_type != nil) {
		response.content_type = [content_type UTF8String];
	}
	if (state.data != nil && [state.data length] > 0) {
		const uint8_t *bytes =
			static_cast<const uint8_t *>([state.data bytes]);
		response.body.assign(bytes, bytes + [state.data length]);
	}
	if (error_message != nil) {
		response.error = [error_message UTF8String];
	}
	completed_.push_back(response);
}

RaHttpTransportResult RaMacHttpClient::ClassifyError(
	const MacHttpTaskState& state, NSError *error) const
{
	if (state.oversize) {
		return RaHttpTransportResult::Oversize;
	}
	if (state.canceled) {
		return RaHttpTransportResult::Canceled;
	}
	if (![[error domain] isEqualToString:NSURLErrorDomain]) {
		return RaHttpTransportResult::ClientError;
	}

	switch ([error code]) {
	case NSURLErrorTimedOut:
	case NSURLErrorCannotFindHost:
	case NSURLErrorCannotConnectToHost:
	case NSURLErrorNetworkConnectionLost:
	case NSURLErrorDNSLookupFailed:
	case NSURLErrorNotConnectedToInternet:
	case NSURLErrorSecureConnectionFailed:
		return RaHttpTransportResult::RetryableClientError;
	case NSURLErrorCancelled:
		return RaHttpTransportResult::Canceled;
	default:
		return RaHttpTransportResult::ClientError;
	}
}

bool RaMacHttpClient::RedirectAllowedLocked(MacHttpTaskState *state,
	NSURLRequest *request)
{
	if (state->request.purpose != RaHttpPurpose::Image) {
		return false;
	}
	if (state->redirects >= 5) {
		return false;
	}
	NSURL *url = [request URL];
	if (url == nil) {
		return false;
	}
	return [[url scheme] isEqualToString:@"https"];
}

NSString *RaMacHttpClient::NSStringFromStdString(const std::string& value)
{
	return [NSString stringWithUTF8String:value.c_str()];
}

std::unique_ptr<RaHttpClient> CreateMacRaHttpClient(
	const std::string& user_agent)
{
	return std::unique_ptr<RaHttpClient>(new RaMacHttpClient(user_agent));
}

} // namespace Xm8Ra

@implementation Xm8RaMacHttpDelegate {
	Xm8Ra::RaMacHttpClient *owner_;
}

- (instancetype)initWithOwner:(Xm8Ra::RaMacHttpClient *)owner
{
	self = [super init];
	if (self != nil) {
		owner_ = owner;
	}
	return self;
}

- (void)detachOwner
{
	owner_ = nullptr;
}

- (void)URLSession:(NSURLSession *)session
	dataTask:(NSURLSessionDataTask *)dataTask
	didReceiveResponse:(NSURLResponse *)response
	completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler
{
	(void)session;
	if (owner_ != nullptr) {
		owner_->DidReceiveResponse(dataTask, response);
	}
	completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session
	dataTask:(NSURLSessionDataTask *)dataTask
	didReceiveData:(NSData *)data
{
	(void)session;
	if (owner_ != nullptr) {
		owner_->DidReceiveData(dataTask, data);
	}
}

- (void)URLSession:(NSURLSession *)session
	task:(NSURLSessionTask *)task
	willPerformHTTPRedirection:(NSHTTPURLResponse *)response
	newRequest:(NSURLRequest *)request
	completionHandler:(void (^)(NSURLRequest *))completionHandler
{
	(void)session;
	if (owner_ != nullptr) {
		owner_->WillRedirect(task, response, request, completionHandler);
	} else {
		completionHandler(nil);
	}
}

- (void)URLSession:(NSURLSession *)session
	task:(NSURLSessionTask *)task
	didCompleteWithError:(NSError *)error
{
	(void)session;
	if (owner_ != nullptr) {
		owner_->DidComplete(task, error);
	}
}

@end
