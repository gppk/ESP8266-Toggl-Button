You will need to create a cpp header file called `secrets-do-not-commit.h` in which enter the following code:

```
const char* Ssid = "SSID";
const char* Password = "WIFI Password";
int const TogglProjectId{see note 1};    
String const Token{"see note 2"};  
```

Note 1 - You can get this by going to https://track.toggl.com/projects and clicking on any project. it will then be the second ID in the URL, copy this as an integer in between the {}

Note 2 - Go to your toggl profile https://track.toggl.com/profile and scroll to the bottom. You will see API Token. Reveal it and copy it into between the quotes.