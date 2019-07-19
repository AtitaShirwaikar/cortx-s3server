import http.client, urllib.parse
import sys
import datetime
from s3iamcli.util import sign_request_v2
from s3iamcli.util import sign_request_v4
from s3iamcli.util import get_timestamp
from s3iamcli.conn_manager import ConnMan
from s3iamcli.error_response import ErrorResponse
from s3iamcli.get_temp_auth_credentials_response import GetTempAuthCredentialsResponse
from s3iamcli.config import Config

class TempAuthCredentials:
    def __init__(self, iam_client, cli_args):
        self.iam_client = iam_client
        self.cli_args = cli_args

    def create(self):
        if(self.cli_args.account_name is None):
            print("Account Name is required for getting auth credentials")
            return

        if(self.cli_args.password is None):
            print("Account password is required for getting auth credentials")
            return

        url_parse_result  = urllib.parse.urlparse(Config.endpoint)
        epoch_t = datetime.datetime.utcnow();
        if(self.cli_args.name is None):
            if(self.cli_args.duration is None):
                body = urllib.parse.urlencode({'Action' : 'GetTempAuthCredentials',
                    'AccountName' : self.cli_args.account_name, 'Password' : self.cli_args.password})
            else:
                body = urllib.parse.urlencode({'Action' : 'GetTempAuthCredentials',
                    'AccountName' : self.cli_args.account_name, 'Password' : self.cli_args.password, 'Duration' : self.cli_args.duration})
        else:
            if(self.cli_args.duration is None):
                body = urllib.parse.urlencode({'Action' : 'GetTempAuthCredentials',
                    'AccountName' : self.cli_args.account_name, 'Password' : self.cli_args.password, 'UserName' : self.cli_args.name})            
            else:
                body = urllib.parse.urlencode({'Action' : 'GetTempAuthCredentials',
                    'AccountName' : self.cli_args.account_name, 'Password' : self.cli_args.password, 'Duration' : self.cli_args.duration, 'UserName' : self.cli_args.name})

        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t, url_parse_result.netloc,
            Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);
        if(headers['Authorization'] is None):
            print("Failed to generate v4 signature")
            sys.exit(1)
        response = ConnMan.send_post_request(body, headers)
        if(response['status'] == 201):
            credentials = GetTempAuthCredentialsResponse(response)
            credentials.print_credentials()
        elif(response['status'] == 503):
            print("GetTempAuthCredentials not successful")
            print("An error occurred (503) when calling the GetTempAuthCredentials operation : " + response['reason'])
        else:
            print("GetTempAuthCredentials not successful")
            error = ErrorResponse(response)
            error_message = error.get_error_message()
            print(error_message)
