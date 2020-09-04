import tweepy

import twitterkey
def SetupTwitter():

        # Authenticate to Twitter
        auth = tweepy.OAuthHandler(twitterkey.consumer_key,twitterkey.consumer_secret_key)
        auth.set_access_token(twitterkey.AccessToken,twitterkey.AccessTokenSecret )

        # Create API object
        api = tweepy.API(auth)
        return api

api = SetupTwitter()
api.update_status("Hello Tweepy hello4")
