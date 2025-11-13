/opt/homebrew/Cellar/openai-whisper/20250625/libexec/lib/python3.13/site-packages/whisper/transcribe.py:132: UserWarning: FP16 is not supported on CPU; using FP32 instead
  warnings.warn("FP16 is not supported on CPU; using FP32 instead")
Detecting language using up to the first 30 seconds. Use `--language` to specify the language
Detected language: English
[00:00.000 --> 00:02.060]  you
[00:30.000 --> 00:55.000]  Okay, I think we can probably start. So the plan is there are four people that were already here yesterday is going to be the same. So I'm going to go through the
[00:55.000 --> 01:16.000]  root files that are produced by px2cf and go through every block. Oops, I got the wrong one for I forgot to change the title. I'm going to go through the
[01:16.000 --> 01:42.000]  The plots that are produced by px2cf and I really forgot to change a bunch of stuff. I realize it now. Okay, yeah, so, so I'm going to update the slide so
[01:43.000 --> 01:56.000]  You notice there are a bunch of tools because I just did a copy and paste. But anyway, the idea is that I will describe the output that is produced by the PS full test and so the quick test just a subset of
[01:56.000 --> 02:12.000]  The steps run by the full test will focus only the px2cf part. And so if you need extra material
[02:12.000 --> 02:25.000]  And here I did a very nice job in updating these formula sort of wiki that contains a bunch of information so that might be useful in case
[02:25.000 --> 02:46.000]  In case you would like to deeper to dig a little bit deeper into the understanding of the modules or the chip so you can find a module and so on. So the commands are here. You notice I really messed up with the copy and paste, but you should know it quite well we have a quick test and a full test.
[02:47.000 --> 03:08.000]  So during the when we run a calibration we actually produce two files, one that contains the calibration results and the metadata and a second one that instead contains the monitor so the evolution of time of some of the values that we are monitoring.
[03:09.000 --> 03:16.000]  So these two files are produced separately because they have two different time spans.
[03:16.000 --> 03:28.000]  One, so the result file is produced from the start to the stop, one is that the monitor file is produced from the configure to the hold or the destroy.
[03:28.000 --> 03:51.000]  And when you run px2cf by the command line or by gift, basically there is no difference, but instead, for example, the burning boxer, the run time is only during the basically the plateau of the temperature, but we still want to monitor the
[03:52.000 --> 04:00.000]  the other information over the all temperature cycles also when there is a transition of temperature.
[04:00.000 --> 04:03.000]  Fabio, do you want questions at the end?
[04:03.000 --> 04:11.000]  Yeah, I was going to say right away so I just interact me if you have any questions.
[04:11.000 --> 04:14.000]  So it's going to be quite interactive.
[04:14.000 --> 04:24.000]  Okay, so I have a question if is it possible from common light to just have monitor the QM running, even if not test is ongoing and to the running.
[04:24.000 --> 04:39.000]  Is there any way to approach that by common line not really thinking.
[04:39.000 --> 04:51.000]  Not really because the common line at the end of the calibration will issue the whole so we cannot follow up with you about how to do it.
[04:51.000 --> 04:53.000]  Yeah, we can add it.
[04:53.000 --> 05:06.000]  I don't think there is anything out of the box that one can use but I think it should be relatively easy to add something I just wait for an input for keyboard and that's it.
[05:06.000 --> 05:11.000]  Yeah, we can do that.
[05:11.000 --> 05:15.000]  Okay, so.
[05:15.000 --> 05:28.000]  So, just a few things. Let me check. So, yeah, there is a so since was asked already a few times.
[05:28.000 --> 05:41.000]  I can you see I made this a high quality drawing for the location of the left and right hybrids.
[05:41.000 --> 06:01.000]  And their position with respect to the PRH and average of the position of the, as I said, MP is in the module, and also the position of the row and column extreme so the zero zero point and zero nine and then 60 points or this might be useful in case you are looking
[06:01.000 --> 06:24.000]  at where physically your chip or your channels are into the into the module. Okay, so I'm going to start going through the various information that we stole in the root file I'm going to start from the metadata for the people that were here yesterday
[06:24.000 --> 06:30.000]  Apologies because it's going to be a little bit of a petition.
[06:30.000 --> 06:34.000]  So, starting so the metadata.
[06:34.000 --> 06:37.000]  They're storing to the result file.
[06:37.000 --> 06:50.000]  And so at the level of the detector we have information basically about your setup. So we have.
[06:50.000 --> 06:55.000]  We have the name of the user that run the test.
[06:55.000 --> 07:01.000]  This is the user of the computer, the name of the computer itself.
[07:02.000 --> 07:20.000]  The gith tag should just contain the head most important that gith commit that is the one that allow us to point to which comment was used and before is the correct that was user if it is it was running with an auto out of the software.
[07:20.000 --> 07:26.000]  Then the calibration that was run the PS full test for this case.
[07:26.000 --> 07:42.000]  Then I'm going to skip quickly this I'm going to come back later so we have few information about the time that is required for for the calibration so we have a timestamp of the starting point so this as soon as the configuration step starts.
[07:42.000 --> 07:48.000]  Then we have forever sub calibration we have the the starting point.
[07:48.000 --> 07:57.000]  These mainly to understand if there is for any reason your setup is slower to understand if there is one particular calibration that is affecting that.
[07:57.000 --> 08:04.000]  And then the stop timestamp and this is when the issue when the stop is issued.
[08:04.000 --> 08:11.000]  There are so if you make a difference there are a few different a few seconds in a.
[08:11.000 --> 08:24.000]  Difference between the values that you see here and the value that is printed in the terminal because you don't should XML file so there are a few seconds of difference but over this time scale should really matter too much.
[08:25.000 --> 08:29.000]  Yes, so this is a local timer right.
[08:29.000 --> 08:33.000]  It is not you to see.
[08:33.000 --> 08:35.000]  Okay.
[08:35.000 --> 08:51.000]  And then actually something that I said wrong yesterday, we'll have to make a correction so there are these two other information initial detector configuration and the final detector configuration.
[08:51.000 --> 09:04.000]  These tools, the XML file that you run by for some reason when I sit in this code, it doesn't see too much, but I think I.
[09:04.000 --> 09:12.000]  Yeah, I dump it so this is the object string as well for the conventional these are the object string because otherwise you have to make a dictionary.
[09:12.000 --> 09:24.000]  But if you get the object string from the file and you ask to dump, it will dump the content and you can recognize that this is XML file.
[09:24.000 --> 09:35.000]  And we store it both at the beginning of the calibration with initial detector configuration and at the end of the calibration that shouldn't change anything.
[09:35.000 --> 09:41.000]  But just for consistency with all the other we store it at the beginning idea.
[09:41.000 --> 09:50.000]  Okay, moving on at the level of the board, we have the board IP address.
[09:50.000 --> 10:02.000]  And then as for the XML file, we also store the XML file that contains the setting of the board.
[10:02.000 --> 10:21.000]  So if I go quickly for the same reason it's not really shown nicely, but if I go quickly into the, for example, the PS module, you see that the configuration file of the board is pointing to another XML, which is over here.
[10:21.000 --> 10:27.000]  And the file, the content that that the object string is actually this file.
[10:27.000 --> 10:36.000]  As for before, since it is an XML file and the score tries to do some sort of formatting than it is.
[10:36.000 --> 10:46.000]  And so in this case, we need to store it, the initial one, the final one, because we are optimizing some of the parameters on the board.
[10:46.000 --> 10:51.000]  We need to communicate with the module and so we want to see what changes.
[10:51.000 --> 10:56.000]  So if I move between one and another, you might see some difference. There is a zero here.
[10:56.000 --> 11:01.000]  That is actually some information that was added.
[11:01.000 --> 11:05.000]  Okay, moving onwards, the optical group.
[11:05.000 --> 11:12.000]  So there is a name ID. So when you run it manually, you don't really see anything because we're not providing any information.
[11:12.000 --> 11:23.000]  But instead, if you run it with the gift or, or this duck, you will see for the bandina, you will see the module name.
[11:23.000 --> 11:28.000]  So the one that you input in the group.
[11:28.000 --> 11:38.000]  And then we have the initial configuration of the LGBT here is that it's different. So there is no rendering so you kind of recognize it.
[11:38.000 --> 11:43.000]  And as well, there is the final one that is over here.
[11:43.000 --> 11:47.000]  They, they look the same because they're kind of the same.
[11:47.000 --> 11:48.000]  Yes.
[11:48.000 --> 11:53.000]  We do we have any parameter to fill the name ID from command line.
[11:53.000 --> 11:55.000]  No.
[11:55.000 --> 11:59.000]  Okay, so perhaps we can look into adding that.
[11:59.000 --> 12:05.000]  Because we will use the command line and we have the name of the module so we can do the same.
[12:05.000 --> 12:07.000]  Okay.
[12:07.000 --> 12:09.000]  So.
[12:09.000 --> 12:20.000]  Okay, so this is done by sending a configuration command via this duck and gift.
[12:20.000 --> 12:24.000]  It is a bit more complicated than that when you have more than one module.
[12:24.000 --> 12:29.000]  When you have one is easy to be done when you have more you have to do one sort of a month.
[12:29.000 --> 12:34.000]  We need to do that.
[12:34.000 --> 12:38.000]  I love the look.
[12:38.000 --> 12:51.000]  Then, still at this level, we have the huge idea of the LGBT that is added from the LGBT and the same for the video experts.
[12:51.000 --> 12:53.000]  Okay.
[12:53.000 --> 13:00.000]  Then going on, I'm going to just show you one idea because they are the same information that I store.
[13:00.000 --> 13:07.000]  So at this level, we cool until we add the hybrid ID, but some speech to a CF doesn't interact with that.
[13:07.000 --> 13:09.000]  We don't really include anything.
[13:09.000 --> 13:11.000]  I don't know if I get a little point.
[13:11.000 --> 13:22.000]  Either potato, get that both interact with the database can include that information is a bit redundant since you already have the mapping between the module and the ID into the database.
[13:22.000 --> 13:28.000]  So we don't really need to add it even more here, but there was just a place for that.
[13:28.000 --> 13:31.000]  Then we have also the CAC fuse ID.
[13:31.000 --> 13:33.000]  This is weird.
[13:33.000 --> 13:35.000]  I didn't check that.
[13:35.000 --> 13:41.000]  So we need to check if for some reason we didn't read it properly.
[13:41.000 --> 13:57.000]  And then since also the CAC has the 60 configuration, we also have the initial 30 and the final 30 stored at this level.
[13:57.000 --> 14:04.000]  And then I guess I'm going to just open the SSA work and I'm going to open both.
[14:04.000 --> 14:14.000]  At this level, we also store the name ID, which is the fuse ID, the initial configuration and the final configuration.
[14:14.000 --> 14:21.000]  And if we go to the SSA quickly, it's going to be at the same information ID.
[14:21.000 --> 14:29.000]  So as per Tuesday initial configuration and final configuration.
[14:29.000 --> 14:37.000]  And these are all the metadata that we are storing into the root file.
[14:37.000 --> 14:39.000]  So, Fabio, just one quick question.
[14:39.000 --> 14:46.000]  This TXT, it means that when we do alignment procedures or something like that, in the final, they are going to be different than in the beginning.
[14:46.000 --> 14:47.000]  Correct.
[14:47.000 --> 14:50.000]  So, for example, let's take one as a say.
[14:50.000 --> 14:58.000]  So I take the first one, so the initial one, and you see that, let me see.
[14:58.000 --> 15:05.000]  Yes, the three minga, everything is F, that is the initial value.
[15:05.000 --> 15:18.000]  And instead, if I take the one afterwards, you see that instead all these values are different, these are the values that are used, that are obtained after the training.
[15:18.000 --> 15:25.000]  Okay.
[15:25.000 --> 15:32.000]  Then I'm going to close everything.
[15:32.000 --> 15:34.000]  So it's going to take a little bit of time.
[15:34.000 --> 15:50.000]  Otherwise, in front of the messy.
[15:50.000 --> 16:06.000]  Okay, then we really start with all the steps of the full test, since the full test is just an extension, a quick test, I just go through all of these and you should get already all the information that you need for the quick test.
[16:06.000 --> 16:23.000]  So, okay, the configuration, we just read the settings from the TXT and XML files, and then we store, we load all the registers into the board and the ASIC.
[16:23.000 --> 16:27.000]  So this pretty straightforward, we don't really store anything.
[16:27.000 --> 16:32.000]  Then the next step is to tune the LPGVT VRF.
[16:32.000 --> 16:44.000]  This VRF is the one that used for the ADC, so in order to provide meaningful value from the ADC.
[16:44.000 --> 16:49.000]  These, in the past, it was a tune, but we didn't really change the name.
[16:49.000 --> 17:05.000]  And now, instead, now, since quite some time, the LPGVT group provided a long TXT file that contains a bunch of information ordered by the FuseID or for the LPGVT that I produced.
[17:05.000 --> 17:17.000]  So what we do is that we just go through the full list, we match the LPGVT based on the FuseID that we read, and then we read from that file the VRF that we should use.
[17:17.000 --> 17:28.000]  These also include a few other information, one with some calibration parameters that then are used for converting the ADC value into voltages.
[17:28.000 --> 17:39.000]  So at this level, we don't store any information, we just take this value from the VRF and we load into the LPGVT.
[17:39.000 --> 17:46.000]  Then, this is actually a step that is not done into the quick test because it's relatively long.
[17:46.000 --> 17:55.000]  So both the SSA and NPA, they also have an ADC that also need to be calibrated.
[17:55.000 --> 18:06.000]  These, I'm wrong, but these at the moment assume a band gap that is, so all the calibration is based on the band gap.
[18:06.000 --> 18:31.000]  The band gap is assumed to be the same value for all NPA and SSA, so we're still waiting for the chip developer to provide us an equivalent of what is provided for the LPGVT in order to have a more refined band gap and I think also VRF such that we can go directly uploading that.
[18:32.000 --> 18:44.000]  Still, this calibration will be run because there are a set of biases based on voltages within the chip that makes the chip work as expected.
[18:44.000 --> 18:58.000]  In particular, make the amount of electron that you're injecting, the one that is actually expected and therefore these steps is optimizing all these parameters.
[18:58.000 --> 19:16.000]  So I don't think there is really too much to show into the plots, but if I go quickly to one of them, we are just storing the VRF that we found after calibrating the ADC VRF.
[19:16.000 --> 19:29.000]  And then we store also the slope of the conversion between the ADC DAC value into ADC voltage.
[19:29.000 --> 19:35.000]  So this is just one slope that we store and then is used for all the ADC conversion.
[19:35.000 --> 19:56.000]  So one thing that you need to consider is that before running this test, your output from the ADC, so basically the monitor, will not be super precise, will be reasonably precise, but not super, super precise because you still need to do this step.
[19:56.000 --> 20:01.000]  Okay, I just gonna move on, but interacting if you have any questions.
[20:01.000 --> 20:04.000]  So then with the...
[20:04.000 --> 20:13.000]  Sorry, the temperature that is read from the MPA chips is going through this calibration here.
[20:13.000 --> 20:14.000]  Yes.
[20:14.000 --> 20:16.000]  Okay.
[20:16.000 --> 20:30.000]  Okay, so one thing I may be missing is right now, if I understand we don't have yet or perhaps we start having some module for which the MPA temperature sensors were calibrated.
[20:30.000 --> 20:37.000]  I'm not sure if this calibration data is already available anywhere, but I wonder if it is clear.
[20:37.000 --> 20:54.000]  I mean, if you're going to change these parameters compared to what they were when they calibrated this temperature sensor, if then we have really a way to reconstruct which calibration we should use for the overall bias.
[20:54.000 --> 21:03.000]  Because my understanding is this MPA, this temperature sensor, before they get calibrated, they have a very large zero uncertainty, if you want to say.
[21:03.000 --> 21:04.000]  Yes.
[21:04.000 --> 21:06.000]  So you don't know the...
[21:06.000 --> 21:09.000]  It's just low, and you don't know...
[21:09.000 --> 21:14.000]  Well, you know there's low very well, you don't know the intercept very well.
[21:14.000 --> 21:15.000]  That is...
[21:15.000 --> 21:16.000]  Okay.
[21:16.000 --> 21:23.000]  So that information, it comes from the wafer testing.
[21:23.000 --> 21:24.000]  So when...
[21:24.000 --> 21:34.000]  So the chuck that was used for doing the wafer testing is kept to a reasonable temperature, I think it's 20 degrees.
[21:34.000 --> 21:39.000]  And then what they do during the steps that they lead, they DC...
[21:39.000 --> 21:49.000]  Sorry, sorry, the voltage corresponding to that temperature sensor, and therefore we can have the offset point.
[21:49.000 --> 22:06.000]  In this moment, this is not stored anywhere in the chip, it needs to be stored into a file that the chip developer needs to provide us, such that we can plug it in and use it in order to set the correct load for every chip.
[22:06.000 --> 22:07.000]  We don't...
[22:07.000 --> 22:12.000]  Okay, so what we get is the voltage from them corresponding to a given temperature.
[22:12.000 --> 22:13.000]  Correct.
[22:13.000 --> 22:15.000]  Okay, so not the DC value.
[22:15.000 --> 22:18.000]  So if we do this calibration, then it's even better.
[22:18.000 --> 22:19.000]  Okay.
[22:19.000 --> 22:20.000]  Thank you.
[22:20.000 --> 22:21.000]  No problem.
[22:21.000 --> 22:22.000]  Well, the volt...
[22:22.000 --> 22:33.000]  I'm not 100% sure that provide already the voltage or the DC value, so not after applying the calibration, not after applying basically this load.
[22:33.000 --> 22:39.000]  So these are not 100% sure what they are going to provide, but at the end of the day, the day should be the same, right?
[22:39.000 --> 22:43.000]  Because it is just as low as between the two.
[22:43.000 --> 22:46.000]  So just an extra conversion.
[22:46.000 --> 22:47.000]  Is anyway...
[22:47.000 --> 22:50.000]  Is the red be at the same ADC?
[22:50.000 --> 23:00.000]  So it just depends if they store the low ADC or if they store the ADC after applying already the conversion.
[23:00.000 --> 23:01.000]  Okay, but...
[23:01.000 --> 23:03.000]  Okay, then perhaps we'll take it offline.
[23:03.000 --> 23:07.000]  I may be a bit more confused about that.
[23:07.000 --> 23:15.000]  So I don't think it is something that they can output on a pad and then measure it with a volmeter.
[23:15.000 --> 23:21.000]  Do you have any microadmipa if I'm wrong?
[23:21.000 --> 23:28.000]  I was trying to refresh my memory because now it's a long time that we discussed this and we didn't have any news.
[23:28.000 --> 23:33.000]  But so I think they will provide us the real band cap measurement.
[23:33.000 --> 23:39.000]  And with that one, we can calibrate more precisely our VRF.
[23:39.000 --> 23:51.000]  And then I don't remember if they will provide us already the starting point of VRF that is giving the correct 850 millivolts.
[23:51.000 --> 23:58.000]  Because with the previous step, we are just setting the range of the ADC.
[23:58.000 --> 24:04.000]  And then with the second step, we are doing the real slope calibration.
[24:04.000 --> 24:08.000]  I think they're going to provide us already the VRF.
[24:08.000 --> 24:12.000]  And actually, I think they even store it into the fuses.
[24:12.000 --> 24:21.000]  Yeah, the VRF can read it from the fuses AD and should be the one that gives us the correct range.
[24:21.000 --> 24:33.000]  Because if you go back to the other plot, basically if we change VRF, we change the range in millivolts of the ADC.
[24:33.000 --> 24:42.000]  And here we set the VRF in ADC such that the range in millivolts is at 850.
[24:42.000 --> 24:50.000]  And they also measure the temperature, but I don't think they have the possibility to output any decent voltage.
[24:50.000 --> 24:55.000]  So they read the temperature, but still we have the same ADC.
[24:55.000 --> 25:03.000]  But the difference between all the other tests is that in this case, the wafer is on a chuck at a fixed temperature.
[25:03.000 --> 25:08.000]  So we know at that temperature what is either the ADC or the voltage that is being built.
[25:08.000 --> 25:19.000]  If they provide the low ADC or if they provide it after applying this curve.
[25:20.000 --> 25:25.000]  Okay, so we direct the light here scan.
[25:25.000 --> 25:31.000]  So let me go back and close a few things.
[25:31.000 --> 25:43.000]  So what we do here is that we, so let me go a little closer, so it's more clear.
[25:43.000 --> 26:01.000]  So here we mentioned the power that the SFP sees on the FC7 in micro-watt changing the modulation of the optical output and the bias of the optical output.
[26:01.000 --> 26:12.000]  So if you look into this direction, this is quite obvious, you increase the bias and therefore you increase the power that is seen into the SFP.
[26:12.000 --> 26:30.000]  So the modulation goes on the other direction and the reasons that the bias sets the high value of the optical output and instead the modulation sets this wing down.
[26:30.000 --> 26:38.000]  And since it's an average, basically increasing the modulation, increase the swing down and the average of the power goes down.
[26:38.000 --> 26:42.000]  So that's why you see this.
[26:42.000 --> 26:55.000]  So from this plot, from the point of view of the QA, you can see if there are some.
[26:55.000 --> 27:03.000]  So you can use the overall value to see if for any reason you have an extra attenuation that you don't expect.
[27:03.000 --> 27:18.000]  That is a little bit more complicated to get because it might depend slightly on the module and that I would say very likely if you have not enough output power, then you're going to see other type of problems.
[27:18.000 --> 27:22.000]  But this will tell us if the output is within a certain range.
[27:22.000 --> 27:33.000]  And also, you can check if there are problems related to one or the two settings that you can set on the laser driver.
[27:33.000 --> 27:44.000]  So if it's flat on one direction on the other, this might indicate that you cannot actually set the modulation of the bias for that particular.
[27:44.000 --> 27:50.000]  Okay, then I open interest. So this was discussed quite a lot.
[27:50.000 --> 28:04.000]  So the FPGVT has the capability to do these eye openings can basically checks every time.
[28:04.000 --> 28:20.000]  So it counts every time the signal that is receiving from the interacts already converted into electrons is between two values, the high value and the low values.
[28:20.000 --> 28:27.000]  And so the result that all the center of the curve is going to fill with the higher counts.
[28:27.000 --> 28:32.000]  I forgot to put the crawler.
[28:32.000 --> 28:46.000]  Because it's all the center is going to be with a higher count and all the area in which the signal is not within the high and low value is going to have a low count.
[28:46.000 --> 28:51.000]  So what is going to see at the end is going to look like the eye open.
[28:51.000 --> 29:04.000]  So for this one, the known problems that in after testing with some devices that were done by Atlas, they saw high effect.
[29:04.000 --> 29:15.000]  So for all their correction counters, when these crossing or the eye was moving towards high value or low value.
[29:15.000 --> 29:22.000]  So this is the known problem that we should know by now.
[29:22.000 --> 29:34.000]  There is another thing to keep into account the FPGVT has the capability to attenuate the signal coming from the interacts and you can attenuate it to one third.
[29:34.000 --> 29:42.000]  That is this plot, two third and that is this one and not attenuated at all.
[29:42.000 --> 29:49.000]  So for the time being, there is not a clear recipe, which one we should look for.
[29:49.000 --> 29:52.000]  So for just completeness, we run all three of them.
[29:52.000 --> 30:00.000]  For what I saw and for also my understanding, most of the issues comes from the highest attenuation.
[30:00.000 --> 30:02.000]  That is also the default one.
[30:02.000 --> 30:13.000]  And therefore, this is the thing that they're going to change in a new version to avoid that you start in the worst conditions that you start with not a mission at all.
[30:13.000 --> 30:18.000]  And they should provide more stability.
[30:18.000 --> 30:22.000]  Okay, then we have to start with the alignment.
[30:22.000 --> 30:27.000]  So this probably the more complicated part.
[30:27.000 --> 30:36.000]  So if I go to this slide, so this is just a quick representation of how the module look like.
[30:36.000 --> 30:42.000]  So in the module, you have an LPGVT and two hybrids, each one with the front end hybrid.
[30:42.000 --> 30:49.000]  There's a session with the CSC and then each of the CSC is connected to eight MPAs.
[30:49.000 --> 30:59.000]  And then each SSA is paired with his own MPA, we are level one and cluster lines.
[30:59.000 --> 31:11.000]  And then on top of that, all these need to communicate with the FPGA and then we need to be able to reconstruct what the CSC is sending to the LPGVT.
[31:11.000 --> 31:22.000]  So the signal is then splitting to hybrids and then each hybrid information is split back into the lines that are in output from the CSC.
[31:22.000 --> 31:25.000]  So now we need to align everything.
[31:25.000 --> 31:31.000]  And the first step, so to make sure that we can properly understand all the communication.
[31:31.000 --> 31:37.000]  So the first step is the alignment between the CSC and LPGVT.
[31:37.000 --> 31:46.000]  In particular here, we are sending a pattern from the data generated by the CSC and we ask the LPGVT to align this pattern.
[31:46.000 --> 32:02.000]  So the LPGVT has an automatic alignment procedure that is going to find basically the center of the eye and therefore identify the best phase in order to read properly the output from the CSC.
[32:03.000 --> 32:16.000]  So these steps, the information are stored at the level of the optical group and there are three plots that I'm going to open.
[32:16.000 --> 32:38.000]  So in order to be sure that we can align reliably the LPGVT on the CSC output, this alignment procedure is repeated 100 times.
[32:38.000 --> 32:46.000]  And therefore we check, first of all, what is the alignment efficiency.
[32:46.000 --> 32:51.000]  So we expect that every time we ask the LPGVT to align, the alignment works.
[32:51.000 --> 32:56.000]  If it doesn't, it might indicate that something not great is going on.
[32:56.000 --> 33:08.000]  And so you might need to further inspect the module, in particular the connection between the readout hybrid and the frontenite.
[33:08.000 --> 33:14.000]  Also, every time we run an alignment, we are going to get a possible phase.
[33:14.000 --> 33:18.000]  And the various phases are shown into the plot.
[33:18.000 --> 33:25.000]  So you have basically a distribution of the best phase that every time was identified.
[33:25.000 --> 33:33.000]  And you see that there are two cases in which you have two possible phases that are chosen.
[33:33.000 --> 33:36.000]  One case in which the two phases are close by.
[33:36.000 --> 33:45.000]  Another case in which the phases are far apart, but you need to consider this phase actually scans over two clock cycles.
[33:45.000 --> 33:51.000]  So roughly the distance between two consecutive clock cycles is roughly eight.
[33:51.000 --> 33:54.000]  And therefore basically these two phases are identical.
[33:54.000 --> 34:03.000]  It just means that you have that sometimes you choose one clock cycle and sometimes you choose the following one.
[34:03.000 --> 34:05.000]  So nothing really concerning.
[34:05.000 --> 34:15.000]  Here, I would say if you have any type of problems, you're going to see a long series, a long sequence of vertical phases,
[34:15.000 --> 34:19.000]  meaning that every time you run it just pick randomly.
[34:19.000 --> 34:27.000]  Or the other option is that you're going to see the align the phase stuck at 15, which is the error code,
[34:27.000 --> 34:34.000]  but you are going to see it also into the over here into the failure of the alignment procedure.
[34:34.000 --> 34:42.000]  Then out of all these phases, what we do is that we select the one with the highest frequency and we set that one as our best phase.
[34:42.000 --> 34:44.000]  Just to pick up one that looks reasonable.
[34:44.000 --> 34:49.000]  And the best phase is stored into this plot just for reference.
[34:49.000 --> 34:52.000]  Here, this plot doesn't tell you too much about the QC.
[34:52.000 --> 34:54.000]  Mainly are the other two.
[34:54.000 --> 34:56.000]  Okay.
[34:56.000 --> 34:58.000]  As usual, just stop me.
[34:58.000 --> 35:02.000]  I'm just going to follow and the next step.
[35:02.000 --> 35:14.000]  At this point, we align the CAC to the LPGVT and therefore we can proceed the alignment basically between the CAC and the board.
[35:14.000 --> 35:19.000]  Because this line needs to be understood by the board at this point.
[35:19.000 --> 35:25.000]  And this step is called align board data world.
[35:25.000 --> 35:40.000]  And the idea here is that you're receiving a packet that we need to identify what is the first world of the packet.
[35:40.000 --> 35:46.000]  So just to give you an idea, in particular for the stubs.
[35:47.000 --> 35:51.000]  So the stop packet looks like this.
[35:51.000 --> 35:59.000]  And the first step that we need to do is that we need to identify the first bit of the sequence.
[35:59.000 --> 36:04.000]  So you need to know which is this bit, which is this bit and which is this bit.
[36:04.000 --> 36:11.000]  And therefore you can properly reconstruct the whole pattern after you identify the first bit of the aid.
[36:11.000 --> 36:18.000]  So this is stored at the level of the hybrid.
[36:18.000 --> 36:29.000]  Because if I go back over here, you see that the FPGA thinks in terms of hybrids, not in terms of modules.
[36:29.000 --> 36:33.000]  Let me just close this.
[36:33.000 --> 36:38.000]  And we have two plots.
[36:38.000 --> 36:49.000]  So we need to identify the delay that we need to apply on the world in order to find the first bit.
[36:49.000 --> 36:58.000]  Now there is a little bit of extra complexity here because it depends if you do 10G or 5G.
[36:58.000 --> 37:04.000]  With 10G you have to think in terms of 16 bits and with the 5G you need to think in terms of 8 bits.
[37:04.000 --> 37:11.000]  But the idea is the same. You need to delay the incoming data in order to find the first bit of the aid.
[37:11.000 --> 37:14.000]  So at the end what we store is what is called bit flip.
[37:14.000 --> 37:18.000]  So it's basically the delay in number of bits that you're applying.
[37:18.000 --> 37:24.000]  Here I would say no information for the QC are really available.
[37:24.000 --> 37:29.000]  It's just going to tell you which is the delay that was chosen.
[37:29.000 --> 37:32.000]  So not too much to talk about this plot.
[37:32.000 --> 37:36.000]  It might be slightly more useful instead to look at this other plot.
[37:36.000 --> 37:51.000]  So there are a few instabilities in writing some specific registers into the firmware that is probably more related to IP bus than to our design.
[37:51.000 --> 38:01.000]  And so it is a bit complicated to address but what we do is that we address them via software by retrying particular steps when they fail.
[38:02.000 --> 38:08.000]  And in this case we try up to 10 times and we store the number of retries here.
[38:08.000 --> 38:17.000]  So if you see something with a very high number of retries, then in my indicates some issues.
[38:17.000 --> 38:25.000]  But if you see just one or two retries that is kind of expected due to the disestability that was mentioned before.
[38:26.000 --> 38:39.000]  Okay, so at this point we know that the pattern that is sent by the CSC is properly understood by the FPGA.
[38:39.000 --> 38:48.000]  At this point the next step is that we can actually verify our stable is this link.
[38:48.000 --> 38:55.000]  And to do that, we set the CSC in order to inject constantly patterns on these lines.
[38:55.000 --> 39:00.000]  And then we see how many times we properly constructed those patterns.
[39:00.000 --> 39:08.000]  So these are stored.
[39:08.000 --> 39:18.000]  Okay, so these are stored at the level of the hybrids.
[39:18.000 --> 39:28.000]  So what we do is that we inject several patterns, each one was stored at the level of the hybrids.
[39:28.000 --> 39:40.000]  So what we do is that we inject several patterns, each one with a relatively large amount of bits, and then we do the pattern matching.
[39:40.000 --> 39:46.000]  We store for all these pattern matching results that I'm going to show you, we always store two blocks.
[39:46.000 --> 39:53.000]  One that contains the number of bits that we actually tested and one that contains the error rate.
[39:53.000 --> 40:00.000]  So you see that for the stubs, the big count is quite high, it's 10 to the 8.
[40:00.000 --> 40:05.000]  For the level one is smaller, it's still 10 to the 6.
[40:05.000 --> 40:10.000]  And the main reason is that for the stubs, we managed to put the pattern matching to the firmware.
[40:10.000 --> 40:15.000]  One instead for the level one, this becomes way more complicated.
[40:15.000 --> 40:18.000]  And therefore we're doing this via software.
[40:18.000 --> 40:29.000]  We don't really plan to do it in the firmware anyway because it will require quite a lot of work and not too much of a gain in terms of speed up.
[40:29.000 --> 40:40.000]  So for all the plot that you're going to see in today, you're going to always see that the stubs have a large number of tests at bit with respect to the level ones.
[40:41.000 --> 40:45.000]  So given a number of test bits, then we show also the error rate.
[40:45.000 --> 40:54.000]  So these you should expect basically never to see errors.
[40:54.000 --> 41:05.000]  However, so for the level one, since the matching is done via software, we know that there are some instabilities in writing into a specific file for that we use.
[41:05.000 --> 41:13.000]  So you might see very small amount of errors in the order of 10 to the minus five or something like that.
[41:13.000 --> 41:19.000]  So nothing too much concerning for the stubs for this particular plot and there is any problems.
[41:19.000 --> 41:28.000]  So I think you can safely assume that the stubs pattern matching should be always at zero.
[41:28.000 --> 41:31.000]  Of course, this was tested on a limited sample.
[41:31.000 --> 41:36.000]  So we will see if something else comes up.
[41:36.000 --> 41:49.000]  So after this test, you basically know that the communication between the CAC and the all the way to the board is stable.
[41:49.000 --> 41:57.000]  The next step is then to better align information about the stubs.
[41:58.000 --> 42:02.000]  Because we actually need the next steps.
[42:02.000 --> 42:16.000]  The reason for that, if I go back to the stock packet, you see that the stock packet is not only one bunch crossing long, but these eight bunch crossing all the way up to here.
[42:16.000 --> 42:36.000]  And we know that this point we can identify the first bit of the of each one of these packets, but we still don't know how to identify the first of this packet, which is the one that contains the information that allows them to decode all the other stubs.
[42:36.000 --> 42:40.000]  So this is done by the stock package alignment.
[42:40.000 --> 42:45.000]  And in particular, we are focusing on the bunch person ID.
[42:45.000 --> 42:52.000]  We can basically read this package with a fixed frequency.
[42:52.000 --> 42:59.000]  And so we know how much the bunch person ID should increase between two consecutive leads.
[43:00.000 --> 43:08.000]  And therefore, if we don't see this increasing, it means that we are not applying the core delay in order to identify the first of this package.
[43:08.000 --> 43:13.000]  Of course, if you read the same bits over here, they will not make sense anymore.
[43:13.000 --> 43:27.000]  So, at the end of this procedure, we store one single file, one single plot at the level of the optical group.
[43:27.000 --> 43:29.000]  Here we go.
[43:29.000 --> 43:34.000]  This is the stock package delay.
[43:34.000 --> 43:43.000]  And so here is shown the stock package delay, they can go from zero to seven, zero to seven packets.
[43:43.000 --> 43:50.000]  And the y-axis indicates the hybrid, so left and right.
[43:50.000 --> 43:56.000]  And this is just one, it doesn't really show anything, just show you which point it was selected.
[43:56.000 --> 44:03.000]  We're showing it like this because at the moment, this was implemented into the firmware.
[44:03.000 --> 44:12.000]  We don't foresee that we have two packages that are two delays that have two different numbers on a two hybrid.
[44:12.000 --> 44:16.000]  We have a single register for the overall module.
[44:16.000 --> 44:22.000]  So far, we never saw a module that is not in this situation.
[44:22.000 --> 44:36.000]  And it is kind of reasonable, the length between the lines between the left and right hybrid are relatively short compared to the width of this delay in terms of nanoseconds.
[44:36.000 --> 44:48.000]  So, we are just plotting them both such that we know that if we start seeing some modules that don't respect any more these, but they're still functional, then we know that we need to modify the field.
[44:48.000 --> 45:01.000]  But in general, from this plot, you expect two bits, two bins filled, only one for each of the row, and they should be all the same stock package delay.
[45:01.000 --> 45:15.000]  Okay, so at this point, we know that we proper align all the information that are coming out from the CC all the way to the board.
[45:15.000 --> 45:28.000]  Now, the next step is to align the output of the NPA to the CC.
[45:28.000 --> 45:40.000]  So the CC need to sample the NPA output so we can set a phase in order to properly sample the output such that the CC can understand what is received.
[45:40.000 --> 45:56.000]  To do that, the CC implements basically the same idea of the LPGVT so as an automatic phase alignment procedure that indicates the best phase that is in the center of the arc.
[45:56.000 --> 46:04.000]  So the plots that I'm going to show you are very similar to the same to the plots I was showing you for the LPGVT.
[46:04.000 --> 46:10.000]  And since these are a hybrid level procedure, we just already level the hybrid.
[46:10.000 --> 46:28.000]  So these are the plots.
[46:28.000 --> 46:42.000]  So as for the LPGVT, we repeat the alignment, I think was on the stage 100 times, and then at every iteration, we can ask the CC if the alignment succeeded.
[46:42.000 --> 46:58.000]  And this is plot into this plot where we show the locking efficiency of the CC out of the 100 test as a function of the NPA AD and as a function of the lines.
[46:58.000 --> 47:05.000]  So we have one level one lines and five step lines that are these one over here.
[47:05.000 --> 47:13.000]  In this case, for what I saw, again, statistic is limited, but if the locking efficiency works, it always works.
[47:13.000 --> 47:21.000]  So you should expect to have a content that is always 100% for these bits.
[47:21.000 --> 47:32.000]  Then as for the LPGVT, since we repeat the scan multiple times, we do forever this line of distribution of the phase that was identified, the best phase.
[47:32.000 --> 47:35.000]  If I zoom in, you can clearly see all of them.
[47:35.000 --> 47:40.000]  So this is for the first NPA, and then we start for the second NPA.
[47:40.000 --> 47:45.000]  And the idea is the same that we have for the LPGVT.
[47:45.000 --> 47:50.000]  So we choose the one that has the highest frequency to be our best phase.
[47:50.000 --> 48:03.000]  And to show which was the phase that was actually selected here is plotted again as a function of the NPA and the step line, the best phase that was selected.
[48:03.000 --> 48:11.000]  In this case, I don't think you can really understand too much what is if there is a failure.
[48:11.000 --> 48:14.000]  So this is just mainly for reference.
[48:14.000 --> 48:20.000]  As for the LPGVT, locking efficiency, mine became some issues.
[48:20.000 --> 48:38.000]  And as well as phase 15 is a error code or a long list of possible phases, which means that the SSC was just picking up a random phase.
[48:38.000 --> 48:50.000]  Okay, so now we recognize the level of the SSC, the correct sampling phase, in order to identify the...
[48:50.000 --> 48:55.000]  to properly reconstruct basically bit one from bit zeroes.
[48:55.000 --> 49:04.000]  Then there is an extra step needed here because the SSC is not just taking information from NPA and send them out, it will elaborate them.
[49:04.000 --> 49:13.000]  And in particular for the STAPs, we need to properly identify the STAP packets.
[49:13.000 --> 49:28.000]  And the STAP packet that is sent by the NPA is something that looks like that, in which we have up to five STAPs storing two voltage one of 8 bits.
[49:28.000 --> 49:36.000]  Also in this case, we need to find the first of the 8 bits, and therefore we do again another word alignment.
[49:36.000 --> 49:45.000]  For this case, we send NPA in order to inject a specific pattern, and then we tell to the LPGVT that pattern.
[49:45.000 --> 49:57.000]  So the LPGVT knows which pattern you need to expect, and we'll basically do an automatic scan of a bit relay in order to properly reconstruct that pattern.
[49:58.000 --> 50:07.000]  So the plot that we store for this STAP is this one, the word alignment.
[50:07.000 --> 50:18.000]  And as a function of the NPA ID and the STAP line, so here is only for the STAP line, there is no word alignment for the level one lines,
[50:18.000 --> 50:26.000]  because it's not needed, there is a header and the SSC uses the header to identify the correct packet.
[50:27.000 --> 50:36.000]  We store the phase. Unfortunately, the plot doesn't look really impressive because phase zero is also a possible value,
[50:36.000 --> 50:47.000]  and if you do a set of in content zero to a plot, it just looks empty, but phase 15 would be a problem.
[50:48.000 --> 50:58.000]  No, sorry, forget what I said, there is no 15 here, so any value is valid, so from zero to seven.
[50:58.000 --> 51:08.000]  Okay, just I keep going, can you just confirm you can hear me or not?
[51:08.000 --> 51:09.000]  Yeah, yeah, we can.
[51:09.000 --> 51:13.000]  Okay, thank you.
[51:13.000 --> 51:23.000]  Okay, at this point, we have everything set up to work properly, so few extra comments.
[51:23.000 --> 51:32.000]  We, at the moment, we don't do an alignment between the SSN NPA because there is just a physical two phase possibility,
[51:32.000 --> 51:37.000]  and so far we found that one of the two phases always works.
[51:37.000 --> 51:43.000]  So we are not doing any extra alignment between these two.
[51:43.000 --> 51:52.000]  Of course, if in a future we start seeing problems, we are going to introduce something, but it doesn't look like to be the case at the moment.
[51:52.000 --> 52:02.000]  Another step that I pleased on is that we, before we're starting UI, just need to find the first bit, but this is basically the same for the CSS,
[52:02.000 --> 52:13.000]  so we have that the packet is divided into two bunch crossing, so we should identify the first one of these two.
[52:13.000 --> 52:24.000]  And for these, we technically had a procedure that is called the BX0 alignment, but up to now, we find out that the value that is done by the procedure,
[52:24.000 --> 52:32.000]  that in theory full work, this automatic procedure by the CRC, doesn't seem really to provide the correct one.
[52:32.000 --> 52:46.000]  In particular, it always inverts the lowest bit of the register that sets this delay, which means that is identifying the baron of these two packets.
[52:46.000 --> 52:53.000]  So for the time being, we're not doing it, and we're setting to a static value.
[52:53.000 --> 53:05.000]  And however, in the past days, the PISA group mentioned that when they go cold, they stop seeing the data that are going through the stub line.
[53:05.000 --> 53:10.000]  So this might be something that we have to look back into it.
[53:10.000 --> 53:22.000]  However, after asking them to run a few tests, it doesn't really seem to be the case to be related to this problem, but it's something that we might need to add back on the procedure.
[53:22.000 --> 53:32.000]  For the time being, we still don't have it, we need to understand better what is the reason, it might be related or it might be this, and we'll just not understand what is going on.
[53:32.000 --> 53:36.000]  Okay.
[53:36.000 --> 53:58.000]  So going back to what I was saying, so if you just skip these extra two steps that I just mentioned, in theory we have everything that is needed to completely verify the communication between the module and the FPGA.
[53:58.000 --> 54:08.000]  So the next step is to verify the communication between the CIC and in this case the NPA.
[54:08.000 --> 54:19.000]  So we are checking this line. So what we're doing is that we're setting the NPA in order to send a specific pattern, and we reconstruct the pattern at the level of FPGA.
[54:19.000 --> 54:27.000]  We know that between CIC and FPGVT was already stable, and so we're now going all the way back to the NPA.
[54:27.000 --> 54:44.000]  So the results that we stored are at the level of the hybrid, and since it's a pattern matching, we do again, we still have two plots.
[54:44.000 --> 54:49.000]  One that contains the number of test bits, and one that contains the error rate.
[54:49.000 --> 55:02.000]  So both the two plots are shown as a function on the NPA hybrid in the x-axis, and on the y-axis we show separately the level one line and the stub lines.
[55:02.000 --> 55:16.000]  So here's just a comment. So at this point, we don't really have the possibility to understand if one of these lines, which one of these lines is the cause of an issue, if you see an issue.
[55:16.000 --> 55:31.000]  And the reason is that we require quite a lengthy procedure in order to distinguish them, and this is done actually at the level of the electric chain validation.
[55:31.000 --> 55:36.000]  So you will see actually later how we can distinguish where the problem is coming from.
[55:36.000 --> 55:43.000]  At this point, we just know that one of these connections has a problem.
[55:43.000 --> 55:56.000]  As before, stub matching is done in the firmware, so you have a quite large number of events, a number of bit testers compared to the level one in order not to have a too lengthy calibration.
[55:56.000 --> 55:59.000]  And the error rate should look something like that.
[55:59.000 --> 56:04.000]  Again, the level one sometimes have some instabilities, so you might see some errors.
[56:04.000 --> 56:10.000]  And on some of the real modules I was testing, I also saw some instabilities sometimes on the stubs.
[56:10.000 --> 56:20.000]  So for what we saw, if you have a broken connection, you have quite a large number of events, so 10% or something like that.
[56:20.000 --> 56:35.000]  Sometimes we see an order of one per meal of errors, and these I think are due to some instabilities that we really didn't address at the beginning, because with the first module we were just lucky when I was testing the new procedure and it was working.
[56:36.000 --> 56:47.000]  So something to keep in mind, if you see some low error rate in the order of percent is very likely due to the some instabilities.
[56:47.000 --> 56:51.000]  Try to run it one more time if the error doesn't go away.
[56:52.000 --> 56:57.000]  Okay, contact me and check the wild bones.
[56:57.000 --> 57:05.000]  So one important point is that for the MPA, all these communications are going through a wild bone pair.
[57:05.000 --> 57:12.000]  So if you see errors through these lines, it means that you need to check the wild bones.
[57:12.000 --> 57:18.000]  Okay, so this is quite important because for the strip side, you can just check the noise.
[57:18.000 --> 57:31.000]  For the pixel size, you are going to a chip, so you need to check all these verify steps that allow you to see the problems in the wild bone between the MPA and the hybrid.
[57:34.000 --> 57:45.000]  Okay, and then the really last step of this alignment is the verification and the communication between, say, an MPA.
[57:45.000 --> 57:50.000]  So these lines, also these lines are wild bone and that's why it's important to check them.
[57:50.000 --> 57:57.000]  Or if you notice, all these alignment procedure are done also in the quick test that is meant to check the module before encapsulation.
[57:57.000 --> 58:04.000]  So that's why we include them all because they allow you to spot missing wild bones.
[58:04.000 --> 58:09.000]  So for this one, the idea is kind of similar as before.
[58:09.000 --> 58:19.000]  Unfortunately, it's a bit more complicated, more technical thing, but MPA doesn't allow to bypass itself.
[58:19.000 --> 58:27.000]  So the only way to see what is going on here is to inject real stuff.
[58:27.000 --> 58:39.000]  So basically, we need to inject some channels into the SSA in the corresponding channel to the MPA in order to make use of these lines.
[58:39.000 --> 58:50.000]  And that is even honestly a bit more complicated than that because you see that each line sends, so these are the eight lines between the SSA and MPA for the cluster.
[58:50.000 --> 58:55.000]  So each line sends the cluster through these lines.
[58:55.000 --> 59:00.000]  And these are in order of the strip that is it.
[59:00.000 --> 59:08.000]  So basically, you need to inject eight cluster every time because you need to make sure that you feel all of these.
[59:08.000 --> 59:12.000]  Otherwise, you will never be able to access the higher cluster lines.
[59:12.000 --> 59:14.000]  Okay, these are just the technicality.
[59:15.000 --> 59:35.000]  So the plot that is saved, again, is two plots, one for the number of tested bits.
[59:35.000 --> 59:42.000]  Again, you have more tested bits on the STAPs lines.
[59:42.000 --> 59:51.000]  Okay, these are the cluster lines, but they still go to the STAP lines after the SSA and MPA pass them.
[59:51.000 --> 59:53.000]  And then the level one lines are less.
[59:53.000 --> 59:57.000]  And this is the kind of plot that you should expect.
[59:57.000 --> 01:00:15.000]  So here, in particular, you see that here we are having quite a large number of bad, I mean, high errors.
[01:00:15.000 --> 01:00:17.000]  And these are actually a real one.
[01:00:17.000 --> 01:00:19.000]  So every time I run, I always get the same.
[01:00:19.000 --> 01:00:26.000]  This is an older prototype and I didn't want to invest too much time in picking it, but we just did it on another one a few days ago.
[01:00:26.000 --> 01:00:29.000]  And we actually could spot a missing world bond.
[01:00:29.000 --> 01:00:31.000]  So look at this plot.
[01:00:31.000 --> 01:00:36.000]  You see that these, for example, you have quite a low thing is actually if you run it one more time, it disappears.
[01:00:36.000 --> 01:00:40.000]  So as I was saying, we found some of the stability here and there.
[01:00:40.000 --> 01:00:45.000]  But please look at this because you will tell you immediately which world bond is the missing one.
[01:00:45.000 --> 01:00:51.000]  And whoever really the position is a bit more complicated because you have to look into the manual of the MPA.
[01:00:51.000 --> 01:00:57.000]  We should make some sort of automatic conversion actually should be relatively easy to do.
[01:00:57.000 --> 01:01:04.000]  So if somebody would like to candidate, that would be a very helpful information to extract from this plot.
[01:01:04.000 --> 01:01:07.000]  So you don't need to go and see the manual.
[01:01:07.000 --> 01:01:15.000]  You just know which world bond correspond to this beam.
[01:01:15.000 --> 01:01:17.000]  Okay.
[01:01:17.000 --> 01:01:21.000]  So this is the end of the line part.
[01:01:21.000 --> 01:01:30.000]  So at this point, we know that our module is well aligned between the various ASICs and with the FC7.
[01:01:30.000 --> 01:01:41.000]  And we also know what is the error rate that we have in between the various connections.
[01:01:41.000 --> 01:01:46.000]  I'm going to stop briefly here because this is probably the most complicated part.
[01:01:46.000 --> 01:01:53.000]  Do you have anything you would like to ask at this point?
[01:01:53.000 --> 01:01:57.000]  I'll give you.
[01:01:57.000 --> 01:02:02.000]  The result file that you're showing, is it for the 10G module?
[01:02:02.000 --> 01:02:05.000]  Can you repeat the word?
[01:02:05.000 --> 01:02:10.000]  I mean, the result file you are showing, is it for the 10G?
[01:02:10.000 --> 01:02:13.000]  Yeah, it is a 10G module.
[01:02:13.000 --> 01:02:15.000]  Okay.
[01:02:15.000 --> 01:02:22.000]  So in the phase alignment between the CIC and the LPGBT,
[01:02:22.000 --> 01:02:31.000]  so in the 5G, will there be the 8-tap difference?
[01:02:31.000 --> 01:02:33.000]  In the phase?
[01:02:33.000 --> 01:02:37.000]  Yeah, in the LPGBT and the CIC.
[01:02:37.000 --> 01:02:48.000]  LPGBT and CIC.
[01:02:48.000 --> 01:02:50.000]  This one?
[01:02:50.000 --> 01:02:52.000]  Yeah.
[01:02:52.000 --> 01:02:56.000]  No, because these are still phases.
[01:02:56.000 --> 01:03:09.000]  So the LPGBT splits the clock that is using into 14 phases, 15 phases, okay?
[01:03:09.000 --> 01:03:11.000]  And it's going to scan them.
[01:03:11.000 --> 01:03:16.000]  So if you have the clock speed that is half of the speed,
[01:03:16.000 --> 01:03:20.000]  it's basically that every phase is going to be just twice as long.
[01:03:20.000 --> 01:03:25.000]  So it will keep going from 0 to 15.
[01:03:25.000 --> 01:03:33.000]  If you go to the 2D plot, if you go to the 2D plot.
[01:03:33.000 --> 01:03:37.000]  There is no 2D plot.
[01:03:37.000 --> 01:03:41.000]  Yeah, this one, this one.
[01:03:41.000 --> 01:03:43.000]  Yeah, this one, this one, yeah.
[01:03:43.000 --> 01:03:49.000]  So let's say for the front end hybrid, left one, start line two,
[01:03:49.000 --> 01:03:52.000]  you have the two phases.
[01:03:52.000 --> 01:03:56.000]  You get the two power phases and these are different by eight phases, right?
[01:03:56.000 --> 01:03:58.000]  Yes.
[01:03:58.000 --> 01:04:03.000]  So in the 5G module also, you will expect this kind of tap difference?
[01:04:03.000 --> 01:04:07.000]  Yes, because it is, so the phase is not of its value.
[01:04:07.000 --> 01:04:12.000]  The phase is really 115 of the clock.
[01:04:12.000 --> 01:04:19.000]  So if you go twice the speed, simply the phase is going to be half of what you have.
[01:04:19.000 --> 01:04:23.000]  So it should be always the same because the phase is really,
[01:04:23.000 --> 01:04:30.000]  depends on the clock that you're using.
[01:04:30.000 --> 01:04:38.000]  Yeah, but for the 5G and the 10G, the LPGBT will be in the 640MHz, right?
[01:04:38.000 --> 01:04:39.000]  640MHz, yes.
[01:04:39.000 --> 01:04:43.000]  So this phase is going to always be two clock cycles.
[01:04:43.000 --> 01:04:47.000]  They overvalued the phase from 0 to 15.
[01:04:47.000 --> 01:04:55.000]  So in one case, it's going to be one divided by 640MHz,
[01:04:55.000 --> 01:04:59.000]  and the other case is going to be one divided by 320MHz.
[01:04:59.000 --> 01:05:06.000]  So these steps are also going to change between the two values.
[01:05:06.000 --> 01:05:11.000]  So you are always spending two clock cycles.
[01:05:11.000 --> 01:05:15.000]  It's just that the clock cycles are shorter when you go to 10G,
[01:05:15.000 --> 01:05:20.000]  but the phase scan is always across two clock cycles.
[01:05:20.000 --> 01:05:25.000]  Ah, okay.
[01:05:25.000 --> 01:05:26.000]  Okay, thank you.
[01:05:26.000 --> 01:05:33.000]  No problem.
[01:05:33.000 --> 01:05:38.000]  If anything else, we'll go ahead.
[01:05:38.000 --> 01:05:41.000]  So we'll add up to here.
[01:05:41.000 --> 01:05:47.000]  So now we do the ring oscillator test.
[01:05:47.000 --> 01:05:53.000]  So this is something that was asked to be included by the chip developer.
[01:05:53.000 --> 01:06:02.000]  So they have these oscillators that, what, oscillates?
[01:06:02.000 --> 01:06:06.000]  And they count how many oscillations they have in a fixed amount of time.
[01:06:06.000 --> 01:06:12.000]  The number of oscillations depends on the temperature, the power,
[01:06:12.000 --> 01:06:17.000]  and also the radiation damage, which is not really the case that we're interested into.
[01:06:17.000 --> 01:06:23.000]  And so they can, in case there are bad power distribution,
[01:06:23.000 --> 01:06:27.000]  something like that, then might indicate some issues.
[01:06:27.000 --> 01:06:32.000]  After discussing with the chip designer, we decided that so far,
[01:06:32.000 --> 01:06:39.000]  they didn't really spot any particular problem that was not seen by other type of tests.
[01:06:39.000 --> 01:06:41.000]  So we just include them.
[01:06:41.000 --> 01:06:45.000]  The test is super fast, so it doesn't really matter too much.
[01:06:45.000 --> 01:06:53.000]  And we just provide some limits in potato that we use to set what is considered good.
[01:06:53.000 --> 01:06:59.000]  And this is based on the statistic that they collected from the wafer testing.
[01:06:59.000 --> 01:07:09.000]  So I don't really know what this moment in time, because we don't really have a failure in the case in which we failed,
[01:07:09.000 --> 01:07:12.000]  that we can actually spot on this plot.
[01:07:12.000 --> 01:07:18.000]  So I think you can safely skip them, because when you look at them, it's not really meaningful.
[01:07:18.000 --> 01:07:25.000]  But basically, you have for every chip, you have a set of these ring oscillators.
[01:07:25.000 --> 01:07:28.000]  Actually, two set of ring oscillators.
[01:07:28.000 --> 01:07:31.000]  So you have what is called a delay count.
[01:07:31.000 --> 01:07:33.000]  And the other one, an inverter count.
[01:07:33.000 --> 01:07:35.000]  I don't know the details about that.
[01:07:35.000 --> 01:07:37.000]  They tell you two different information.
[01:07:37.000 --> 01:07:41.000]  I think one of the two is for the temperature and the other one for the radiation damage.
[01:07:41.000 --> 01:07:43.000]  So we don't really care about one of the two.
[01:07:43.000 --> 01:07:46.000]  The information is the marble.
[01:07:46.000 --> 01:07:53.000]  And then for each module, you have, for each MPA, you have a ring oscillator in the periphery.
[01:07:53.000 --> 01:07:56.000]  And one for each row.
[01:07:56.000 --> 01:07:58.000]  In the SSA, it's slightly different.
[01:07:58.000 --> 01:08:00.000]  You have less.
[01:08:00.000 --> 01:08:06.000]  And so for every chip, you have four of them.
[01:08:06.000 --> 01:08:11.000]  And for what MS2, there is a bottom left, bottom center, bottom right, and top right.
[01:08:11.000 --> 01:08:13.000]  So just different location of the chip.
[01:08:13.000 --> 01:08:18.000]  So as I was saying, I think you can ignore them from the time being.
[01:08:18.000 --> 01:08:26.000]  I just told, so I just wanted to mention them also in this tutorial.
[01:08:26.000 --> 01:08:31.000]  Okay.
[01:08:31.000 --> 01:08:33.000]  Then.
[01:08:33.000 --> 01:08:35.000]  So pedestal equalization.
[01:08:35.000 --> 01:08:39.000]  So this one, I think, you know, quite well, gonna be brief.
[01:08:39.000 --> 01:08:46.000]  The idea is that every,
[01:08:46.000 --> 01:09:01.000]  every comparator by construction will always have a slightly different offset because you cannot really set it super precise due to mismatch when you, this comparator implemented to the chip.
[01:09:01.000 --> 01:09:14.000]  And therefore there are various techniques that are used in order to compensate for that. And in particular for both MPN as I say, what is done is that we apply a global threshold.
[01:09:14.000 --> 01:09:21.000]  And then in every channel, we have a local threshold that is added on top of the global one.
[01:09:21.000 --> 01:09:36.000]  And since the local can be set independently from for every channel, you can use it to equalize the threshold for every channel such that you have a uniform threshold.
[01:09:36.000 --> 01:09:44.000]  So here we have a difference between the quick test and the full test. You see that the name are different.
[01:09:44.000 --> 01:09:51.000]  The reason is that in the quick test, we do a binary scan.
[01:09:51.000 --> 01:10:01.000]  However, for since we're using the fast counter without which is the counters that are synchronous, the occupancy is not monotone.
[01:10:01.000 --> 01:10:14.000]  And so we need to be far away from the area of the pedestal. Otherwise you have some, you have a big peak due to the fact that you're reading the events synchronously.
[01:10:14.000 --> 01:10:30.000]  So you count basically many times the event past the threshold and when you're close to the pedestal, you're gonna have a bunch of events that cause that because the noise just making your comparator oscillating up and down constantly.
[01:10:30.000 --> 01:10:37.000]  So you need to inject high value to be far away from the pedestal.
[01:10:37.000 --> 01:10:55.000]  But that doesn't provide your very precise pedestal trimming because it will start playing a game also variation of amplification and amount of charging injected across several channels.
[01:10:55.000 --> 01:11:06.000]  So it is a good approach to have a quick feedback on noisy channels or low noise channel that for the strips indicate were born in this connection.
[01:11:06.000 --> 01:11:20.000]  But if you want something more precise, then allow you to have occupancy measurement at the relatively low threshold, then this method doesn't work anymore.
[01:11:20.000 --> 01:11:35.000]  So in the full test, the full scan instead that is done during the full test, what we do is that rather than doing a binary search, we scan every single offset, which is more lengthy.
[01:11:35.000 --> 01:11:41.000]  But it allows us to avoid the problems due to the fact that the threshold is not monotone.
[01:11:41.000 --> 01:11:44.000]  Sorry, the occupancy versus threshold is not monotone.
[01:11:45.000 --> 01:11:48.000]  The outcome of the two plots are actually the same.
[01:11:48.000 --> 01:11:55.000]  So you don't, so it's kind of transparent from the point of view of the files that they saved.
[01:11:55.000 --> 01:12:01.000]  And these are located at the level of the chips.
[01:12:01.000 --> 01:12:11.000]  In particular, two plots, one that contains the offset.
[01:12:11.000 --> 01:12:20.000]  So this one is the best value, the best local threshold that was identified in order to properly equalize the threshold.
[01:12:20.000 --> 01:12:24.000]  And one is instead the occupancy given the threshold.
[01:12:24.000 --> 01:12:29.000]  So basically we ask a target occupancy to be achieved that is around 50%.
[01:12:29.000 --> 01:12:37.000]  So you should expect something like this with all the channels that will see later on these 50%.
[01:12:37.000 --> 01:12:39.000]  So where to see problems?
[01:12:39.000 --> 01:12:49.000]  So most of the issue will be more clearly visible when you run the next step, this curve, because they do a more complete analysis of their own measurement.
[01:12:49.000 --> 01:12:55.000]  But already from these two plots, you can try to understand already something that might have failed.
[01:12:55.000 --> 01:13:06.000]  So for the offset, since we are trying to target in 50%, if you see some channels that have a very high or low offset, so close to zero, close to one.
[01:13:06.000 --> 01:13:08.000]  Sorry, occupancy.
[01:13:08.000 --> 01:13:15.000]  It means that something failed during the trimming.
[01:13:15.000 --> 01:13:25.000]  Quick note, I realized while doing the slides that we had an error in the label of the y-axis.
[01:13:25.000 --> 01:13:28.000]  This is actually occupancy, not offset.
[01:13:28.000 --> 01:13:31.000]  We'll be fixing the next tag.
[01:13:31.000 --> 01:13:37.000]  And instead also from this plot, here is really the offset.
[01:13:37.000 --> 01:13:41.000]  If you see, so this value can go from zero to 31.
[01:13:41.000 --> 01:13:52.000]  If you see a few channels that are stuck to zero to 31, it means that you didn't have enough range to correct those channels.
[01:13:52.000 --> 01:13:57.000]  Same this for the MPA, for the SSA is kind of the same.
[01:13:57.000 --> 01:14:09.000]  The only difference is that it is shown in 2D, because it's a little bit more easy to be understood, to see their distribution.
[01:14:09.000 --> 01:14:20.000]  And also in this case, also here there is a mistake, it is the wrong columns and the y-axis is the occupancy and the offset.
[01:14:20.000 --> 01:14:25.000]  So the reasonment is kind of the same.
[01:14:25.000 --> 01:14:36.000]  This is not working honestly super great at the moment, so we're trying to understand if we can do a bit better into having a more uniform distribution.
[01:14:36.000 --> 01:14:42.000]  As you see, there are a few that are quite high, around 70%.
[01:14:42.000 --> 01:14:47.000]  One instead for the SSA, this was a little bit more uniform.
[01:14:47.000 --> 01:14:52.000]  It might be simply that we cannot do much better than that for the chip.
[01:14:52.000 --> 01:14:57.000]  So we're going to see if we can improve it a little bit.
[01:14:57.000 --> 01:15:03.000]  Okay, so this is for the pedestal equalization.
[01:15:03.000 --> 01:15:08.000]  And then we have the noise measurement, the scars.
[01:15:08.000 --> 01:15:15.000]  So for the S-carve, okay, this you know quite well.
[01:15:15.000 --> 01:15:20.000]  So we scan the threshold with a given injection and we measure the occupancy.
[01:15:20.000 --> 01:15:25.000]  What you get is that theoretically should have a step function.
[01:15:25.000 --> 01:15:30.000]  But then of course there's noise, so that step function is convoluted with a Gaussian.
[01:15:30.000 --> 01:15:37.000]  And so the final result is a business error function that has a S shape.
[01:15:37.000 --> 01:15:41.000]  So that's why we often refer them to as scarves.
[01:15:41.000 --> 01:15:48.000]  By fitting the scarve, then we can extract back the convoluted Gaussian.
[01:15:48.000 --> 01:15:57.000]  And therefore we can extract the noise and from the center of the convoluted step function, we can extract the duration.
[01:15:57.000 --> 01:16:01.000]  So this stores a few more plots.
[01:16:01.000 --> 01:16:07.000]  So starting from the chip level and in particular from the SSA.
[01:16:07.000 --> 01:16:13.000]  So we store the full S-carve.
[01:16:13.000 --> 01:16:19.000]  And this is also a function of the channel, the threshold that we apply in the occupancy.
[01:16:19.000 --> 01:16:26.000]  And if I do a projection, we are going to see that this actually is an S-carve, an S shape.
[01:16:26.000 --> 01:16:36.000]  So if you want to see all of them and since we fit them, we also store under the channel folder the S-carve with its own fit.
[01:16:36.000 --> 01:16:42.000]  So you can also check if one particular noise or pedestal phone look weird,
[01:16:42.000 --> 01:16:48.000]  but you can come and see if for any reason the fit was failed.
[01:16:48.000 --> 01:16:56.000]  Then still at the level of the chip, we also store the pedestal distribution.
[01:16:56.000 --> 01:17:01.000]  So basically the center of the S-carve that you should see is quite sharp.
[01:17:01.000 --> 01:17:03.000]  So it's quite uniform pedestal.
[01:17:03.000 --> 01:17:14.000]  This is thanks to the trimming as well as the pedestal distribution, but instead across the various channels.
[01:17:14.000 --> 01:17:20.000]  So you can see how it works across the whole SSA.
[01:17:20.000 --> 01:17:23.000]  And we do the same for the noise.
[01:17:23.000 --> 01:17:32.000]  So we show the noise distribution, cumulative, and then the noise per channel.
[01:17:32.000 --> 01:17:39.000]  So you can see that in the channels.
[01:17:39.000 --> 01:17:47.000]  And that's all for the S-carve at the level of the chip, at least for SSA.
[01:17:47.000 --> 01:17:52.000]  Just quickly for MPA, the idea is more or less the same.
[01:17:52.000 --> 01:17:55.000]  The main difference is that some of them are 2D.
[01:17:55.000 --> 01:18:03.000]  So this is the same as before, the S-carve for all the channels that you see is already quite well trimmed.
[01:18:03.000 --> 01:18:04.000]  So not too bad.
[01:18:04.000 --> 01:18:08.000]  We're just trying to see if we can do a bit better.
[01:18:08.000 --> 01:18:15.000]  This is the, sorry, the skipped one, the pedestal distribution.
[01:18:15.000 --> 01:18:22.000]  So you see that these are the kind of taste that we're trying to see if we can improve slightly.
[01:18:22.000 --> 01:18:35.000]  And as for the MPA, we have the channel, where is it?
[01:18:35.000 --> 01:18:36.000]  Can you find it?
[01:18:36.000 --> 01:18:37.000]  Yes.
[01:18:37.000 --> 01:18:46.000]  The channel distribution across the overall, sorry, the pedestal distribution across the overall channel.
[01:18:46.000 --> 01:18:51.000]  Actually, I just realized it would be nice to make this one 2D rather than 1D.
[01:18:51.000 --> 01:18:59.000]  So you can see the distribution across the map of the chip.
[01:18:59.000 --> 01:19:02.000]  Same for the noise.
[01:19:02.000 --> 01:19:15.000]  We have the channel noise distribution for the pixel, as well as the distribution along all the channels.
[01:19:15.000 --> 01:19:18.000]  But in this case, we also have the 2D distribution.
[01:19:18.000 --> 01:19:35.000]  And this one is a little bit more informing because you see that the noise at the edges are bigger because the chip, the pixel over there are larger because they need to keep into account that you cannot put 2As extra to cross to each other.
[01:19:35.000 --> 01:19:45.000]  Also, one other information that you might notice, the two values, the two leftmost values are always the same.
[01:19:45.000 --> 01:19:50.000]  And the reason is that the rightmost chip, in reality, does not really exist.
[01:19:50.000 --> 01:19:53.000]  It just duplicated.
[01:19:53.000 --> 01:20:01.000]  And this is because for simplicity, they need to match the number of strips.
[01:20:01.000 --> 01:20:12.000]  But in reality, the number of real pixels that you have on the NPA, the real number of columns that you have is not 120, but it's only 118.
[01:20:12.000 --> 01:20:15.000]  But at the edge, every chip is duplicated.
[01:20:15.000 --> 01:20:25.000]  So in total, you will see it's from 120 pixels, even if in reality there are less than 120 columns.
[01:20:25.000 --> 01:20:32.000]  Okay, so these are the plots at the level of the chip.
[01:20:32.000 --> 01:20:42.000]  And then we have also other plots that show you the behavior on the overall module that are over here.
[01:20:42.000 --> 01:20:51.000]  So we have the strip noise distribution over the whole hybrid, sorry, not module hybrid.
[01:20:51.000 --> 01:20:55.000]  The same, but just the cumulative distribution.
[01:20:55.000 --> 01:21:03.000]  The pixel channel noise distribution, as well as the cumulative distribution.
[01:21:03.000 --> 01:21:11.000]  So basically the same plot that we're showing at the level of the chip, but just accumulated over the whole hybrid.
[01:21:11.000 --> 01:21:18.000]  And this is all from the, oh yeah, so Alessandro, please go ahead.
[01:21:18.000 --> 01:21:22.000]  I might not see the hands just up and let us show you now.
[01:21:22.000 --> 01:21:25.000]  So just pick up, don't worry about interrupting.
[01:21:25.000 --> 01:21:27.000]  Okay, no, I just raised my hand.
[01:21:27.000 --> 01:21:31.000]  So just a technical question.
[01:21:31.000 --> 01:21:38.000]  How is the conversion between channels and row and columns in the NPA?
[01:21:38.000 --> 01:21:39.000]  Yes.
[01:21:39.000 --> 01:21:47.000]  So I did this very high technical design, high level technical design.
[01:21:47.000 --> 01:21:53.000]  So I indicate in the position, so these are, I forgot to mention, this is the top view.
[01:21:53.000 --> 01:22:03.000]  So you have the arrow H, the POH left and right, and the position of the 00 point on the left side is over here.
[01:22:03.000 --> 01:22:15.000]  And therefore this is a 0 960 and is inverted into the right high because they are specular.
[01:22:15.000 --> 01:22:21.000]  Yeah, but then there are some plots, which you have on the NPA, on the NPA folder.
[01:22:21.000 --> 01:22:27.000]  You have plots where there is channels and they goes up to all the pixel that you have.
[01:22:27.000 --> 01:22:34.000]  And for example, if you add the plots, the 2D plots, there are column and rows.
[01:22:34.000 --> 01:22:43.000]  So you just, it's just row times plus the columns or vice versa.
[01:22:43.000 --> 01:22:47.000]  It is basically one row.
[01:22:47.000 --> 01:22:51.000]  And then we stick after that the other row and so on.
[01:22:51.000 --> 01:22:53.000]  So basically you move from left to right.
[01:22:53.000 --> 01:22:57.000]  And then you go to the next row and you go back to right.
[01:22:57.000 --> 01:22:59.000]  Okay, perfect. Thanks.
[01:22:59.000 --> 01:23:01.000]  No problem.
[01:23:01.000 --> 01:23:10.000]  Yeah, we're going to try to include all the plots of the pixel.
[01:23:10.000 --> 01:23:20.000]  I noticed that once we're going through them, some of them, they don't really have the 2D.
[01:23:20.000 --> 01:23:23.000]  So we're thinking we're going to just include also the 2D.
[01:23:23.000 --> 01:23:29.000]  So it is useful also to visualize them to be so you can better understand where those are located.
[01:23:29.000 --> 01:23:36.000]  But it's also useful to see in 1D because sometimes you see some particular trend that in 2D cannot be visible.
[01:23:36.000 --> 01:23:44.000]  Or more 3D to be the same.
[01:23:44.000 --> 01:23:46.000]  Okay, moving on.
[01:23:46.000 --> 01:23:57.000]  So up to here, if you exclude the fact that we also included the ADC calibration and that, oh, I forgot to mention one thing.
[01:23:57.000 --> 01:24:11.000]  Pedestal, so since we have done the full test for the full scan, sorry, for the pedestal, then we can do the noise injection with low injection because we trim better the pedestal.
[01:24:11.000 --> 01:24:18.000]  So we know that the pedestal is more equalizing so we can inject lower noise.
[01:24:18.000 --> 01:24:26.000]  Cannot be too low because of the one you run again to the big peak due to the asynchronous nature of the counters.
[01:24:26.000 --> 01:24:34.000]  But other than that, the steps that we do up to here, excluding the ADC, are the same that is done with the quick test.
[01:24:34.000 --> 01:24:39.000]  So basically up to now we just discussed the quick test part.
[01:24:39.000 --> 01:24:46.000]  Now we're going to move on the thing that we just do on the full test.
[01:24:46.000 --> 01:24:50.000]  And the first one is the injection delay optimization.
[01:24:50.000 --> 01:25:05.000]  So here you rely on the fact that the results of the quick test are already somewhere and you pulling in the trims from the quick test.
[01:25:05.000 --> 01:25:08.000]  No, we do it from scratch every time.
[01:25:08.000 --> 01:25:15.000]  But then how do you inject the lower charge and expect things to work?
[01:25:15.000 --> 01:25:20.000]  No, in the quick test we don't do the low charge injection.
[01:25:20.000 --> 01:25:23.000]  If you turn, we do the full scan.
[01:25:23.000 --> 01:25:25.000]  So the one that Kevin developed.
[01:25:25.000 --> 01:25:28.000]  Oh, so this is really just one by one.
[01:25:29.000 --> 01:25:45.000]  But in principle, you can limit the range of that and improve timing further if you already take results of the quick scan, quick test.
[01:25:45.000 --> 01:25:47.000]  In theory, yes.
[01:25:47.000 --> 01:25:52.000]  The only thing is that you will need to rely on the database.
[01:25:53.000 --> 01:25:59.000]  But what if we run a pedestrian equalization without full scan right before that?
[01:25:59.000 --> 01:26:01.000]  Will that be faster?
[01:26:04.000 --> 01:26:06.000]  A priori, maybe yes.
[01:26:06.000 --> 01:26:14.000]  I don't know how much you will gain on limiting the range of the full scan.
[01:26:14.000 --> 01:26:16.000]  Oh, it's already down to three minutes.
[01:26:16.000 --> 01:26:17.000]  Okay, that's fine.
[01:26:17.000 --> 01:26:18.000]  Thanks.
[01:26:18.000 --> 01:26:19.000]  No, no problem.
[01:26:19.000 --> 01:26:21.000]  So I might check.
[01:26:21.000 --> 01:26:27.000]  I don't know if, so you still, you will add other seconds.
[01:26:27.000 --> 01:26:32.000]  But improving three minutes is not enough.
[01:26:32.000 --> 01:26:40.000]  So I don't think it is a big deal at this point to keep the full test like that.
[01:26:40.000 --> 01:26:42.000]  Okay.
[01:26:42.000 --> 01:26:46.000]  Okay, going back to the delay optimization.
[01:26:46.000 --> 01:26:55.000]  So the reason this point is that up to now we did everything with the injection without the synchronous without.
[01:26:55.000 --> 01:26:58.000]  So you don't need to worry about the injection delay.
[01:26:58.000 --> 01:27:00.000]  You don't need to worry about the latency.
[01:27:00.000 --> 01:27:06.000]  You just inject and the counter is going to count that the comparator went above the pressure.
[01:27:06.000 --> 01:27:10.000]  The signal went above the pressure comparator.
[01:27:10.000 --> 01:27:21.000]  And then, however, we want to do some testing, which we all really inject events and we count the efficiency.
[01:27:21.000 --> 01:27:31.000]  And the reason why this is, I think it's better to be done with synchronous counters that with a synchronous, especially when you go low in a threshold.
[01:27:31.000 --> 01:27:39.000]  You don't know exactly what is your denominator because the synchronous might count three, four times into the same bunch crossing.
[01:27:39.000 --> 01:27:43.000]  Because you have the nature of the synchronous without.
[01:27:43.000 --> 01:27:49.000]  So instead of with the synchronous, you always know how many events you injected.
[01:27:49.000 --> 01:27:58.000]  But in order to do that, you have to make sure that once you inject, you are also sampling the correct point of the pulse shape.
[01:27:58.000 --> 01:28:05.000]  And in order to do that, we do a scan of the injection delay.
[01:28:05.000 --> 01:28:07.000]  Why I close it, I have no idea.
[01:28:07.000 --> 01:28:09.000]  Let me open it back.
[01:28:19.000 --> 01:28:28.000]  And so we do a scan injecting of the occupancy.
[01:28:28.000 --> 01:28:36.000]  Okay, yeah. So what we do here is that for each one of these pointing, we are delaying the injection at a certain amount.
[01:28:36.000 --> 01:28:43.000]  And then we find the threshold at which we get a 50% occupancy.
[01:28:43.000 --> 01:28:51.000]  What it means is that when you're 50%, your show is exactly at the height of the signal that you're going to sampling.
[01:28:51.000 --> 01:28:58.000]  And therefore by simply plotting these as a function of the delay, you can actually reconstruct the whole shape.
[01:28:58.000 --> 01:29:05.000]  And then the next step is just to identify the working point.
[01:29:05.000 --> 01:29:14.000]  And this is this point, the second plot that is saved, that for the delay is relatively easy.
[01:29:14.000 --> 01:29:16.000]  This is set at the peak.
[01:29:16.000 --> 01:29:22.000]  And the reason why is that the peak in time in first approximation is dependent on the amount of charge that you inject.
[01:29:22.000 --> 01:29:27.000]  So even if you decide to inject less charge, the peak is going to still be there.
[01:29:27.000 --> 01:29:33.000]  And then from the distance from the pedestal, what we do is that we set it at 5 sigma.
[01:29:33.000 --> 01:29:35.000]  For the time being, we'll choose that value.
[01:29:35.000 --> 01:29:41.000]  It can be discussed to be changed, but I'm just showing you what the H2ACF is doing right now.
[01:29:41.000 --> 01:29:48.000]  So we know that from this point onwards, all the measurement will be done with this threshold and with this injection delay.
[01:29:48.000 --> 01:29:56.000]  If we don't change anything else, we know that every time we inject the module is in a good condition to record the heat.
[01:29:57.000 --> 01:30:01.000]  And this is for the SSA.
[01:30:01.000 --> 01:30:05.000]  For the MPA is exactly the same.
[01:30:05.000 --> 01:30:13.000]  It's slightly different, however, the part shape, but the idea is identical.
[01:30:13.000 --> 01:30:17.000]  You inject, you find a peak, and then you find a pedestal.
[01:30:17.000 --> 01:30:25.000]  You know what is the noise because you just measure with a scale, you raise the pressure by five times that noise and you set the working point.
[01:30:27.000 --> 01:30:35.000]  Okay, so at this point, we can actually do injection measurement because we know that the injection is properly set.
[01:30:35.000 --> 01:30:49.000]  And we do this by injecting three different, sorry, five different amount of charge.
[01:30:49.000 --> 01:30:57.000]  And I'm showing you first with the SSA.
[01:30:57.000 --> 01:31:05.000]  So here it is shown the occupancy for every channel.
[01:31:05.000 --> 01:31:15.000]  And we do it for zero meep, a quarter of a meep, half a meep, one meep and two meep.
[01:31:15.000 --> 01:31:21.000]  And so zero meep, you basically shouldn't see anything because five signals quite high.
[01:31:21.000 --> 01:31:27.000]  So the coupons should be below 10 to the minus five.
[01:31:27.000 --> 01:31:31.000]  And sometimes you see one, but nothing really concerning.
[01:31:31.000 --> 01:31:37.000]  I mean, one hit because the probability is low, but not zero.
[01:31:37.000 --> 01:31:50.000]  Then a quarter of a meep, it comes out to be roughly around 50, a bit more than 50%, because if you do the calculation, you see that is roughly the pressure that we're setting.
[01:31:50.000 --> 01:31:57.000]  Then half a meep is on the percent and as well as one meep and two meep.
[01:31:57.000 --> 01:32:03.000]  And it is quite important to check lower meeps, not to have both radiation damage.
[01:32:03.000 --> 01:32:11.000]  But for the fact that you're going to have a lower, the yours have events with low, is on now distribution.
[01:32:11.000 --> 01:32:17.000]  But most important, I would say, because you care about the cluster size.
[01:32:17.000 --> 01:32:24.000]  And then the same thing is done for the PS.
[01:32:24.000 --> 01:32:37.000]  The only difference for the PS is that instead we show a 2D plot in order to see what the problem, whether it's occurring.
[01:32:37.000 --> 01:32:42.000]  I think, okay, just for simplicity, I open everything like that.
[01:32:42.000 --> 01:32:45.000]  The scale always goes from zero to one.
[01:32:45.000 --> 01:32:55.000]  And as you see, there are a few noise-inch pixels, but the coupons is very low, so nothing really concerning.
[01:32:55.000 --> 01:33:05.000]  With a quarter of a meep, instead you start to see here 100% efficiency, because the pressure is much lower for the pixel.
[01:33:05.000 --> 01:33:10.000]  Thanks to the lower noise and the same for half a meep, one meep, and two meeps.
[01:33:10.000 --> 01:33:17.000]  So just a small comment. You remember before saying this, pixel don't relax.
[01:33:17.000 --> 01:33:23.000]  Every time a heat is duplicated, a heat is created here is duplicated.
[01:33:23.000 --> 01:33:32.000]  So that's why sometimes you see something like that, but we're ignoring everything that is not month, that is not injected at the moment.
[01:33:32.000 --> 01:33:36.000]  So you really need to be a little bit lucky to see something like that.
[01:33:36.000 --> 01:33:43.000]  But the point is just ignore completely the first and the last, because we are not really injecting them.
[01:33:43.000 --> 01:33:50.000]  We are just nothing. These pixels don't exist at all.
[01:33:50.000 --> 01:34:00.000]  So you can just safely ignore them and all the rest that are 100%.
[01:34:01.000 --> 01:34:03.000]  Okay.
[01:34:03.000 --> 01:34:08.000]  Then the common mode noise.
[01:34:08.000 --> 01:34:19.000]  So these.
[01:34:19.000 --> 01:34:29.000]  So common mode noise. These are a lot of blood and I might forget some of them.
[01:34:29.000 --> 01:34:39.000]  Okay. So the idea of the common mode noise that we want to see if there are a correlation between the channels.
[01:34:39.000 --> 01:34:47.000]  Interior expect that every channel is completely independent from the others, but in reality, they belong to the same chip.
[01:34:47.000 --> 01:34:49.000]  They belong to the same hybrid.
[01:34:49.000 --> 01:34:55.000]  They share in common ground and in power.
[01:34:55.000 --> 01:35:00.000]  So they're never going to be perfectly uncorrelated.
[01:35:00.000 --> 01:35:08.000]  So we have, we do in reality two measurements of the common mode noise at two different thresholds.
[01:35:08.000 --> 01:35:15.000]  And these came out after discussing with Giovanni that there was interesting to do some measurements for the integration.
[01:35:15.000 --> 01:35:23.000]  We added a second step, but since the calibration is quite fast, it doesn't really hurt the time that is required.
[01:35:23.000 --> 01:35:25.000]  You see that is 16 seconds.
[01:35:25.000 --> 01:35:27.000]  So what is the trick over here?
[01:35:27.000 --> 01:35:33.000]  So initially what we wanted to do is to maximize the occupancy.
[01:35:33.000 --> 01:35:42.000]  So just to give you the idea, in the West, we said the pressure at the pedestal, such that the average occupancy is 50%.
[01:35:42.000 --> 01:35:54.000]  And we can do that because the 2S and the 2S sends data unsparcified.
[01:35:54.000 --> 01:35:57.000]  So we get one bit for every strip.
[01:35:57.000 --> 01:36:01.000]  And then the CAC can just bypass the specification.
[01:36:01.000 --> 01:36:06.000]  And so you can get the occupancy up to 100%.
[01:36:06.000 --> 01:36:09.000]  So all the strips hit.
[01:36:09.000 --> 01:36:13.000]  In the PS, there is not such an option.
[01:36:13.000 --> 01:36:16.000]  You are limited by the amount of cluster that you can send out.
[01:36:16.000 --> 01:36:26.000]  And in particular, you can send up to 127 strip cluster and 127 picture cluster for the whole hybrid.
[01:36:26.000 --> 01:36:30.000]  So it's definitely not 100% occupancy.
[01:36:30.000 --> 01:36:38.000]  So in order to try to do something as close as possible, the original idea was to the 2S.
[01:36:38.000 --> 01:36:48.000]  The original idea was to inject an amount of certain pressure such that the occupancy is around 50% of the maximum available cluster.
[01:36:48.000 --> 01:36:52.000]  So around 60ish cluster.
[01:36:52.000 --> 01:36:57.000]  And these are the plots that are called occupancy driven.
[01:36:57.000 --> 01:37:05.000]  So in this one, we are trying to have around 50% of the overall occupancy.
[01:37:05.000 --> 01:37:16.000]  And hopefully you are going to see better when I show you at the level of the hybrid.
[01:37:16.000 --> 01:37:18.000]  Kind of.
[01:37:18.000 --> 01:37:25.000]  So we are trying to move these basically as much of the center as possible.
[01:37:25.000 --> 01:37:33.000]  Consider that it's pretty tough because the resolution of the pressure doesn't allow you to do super fine adjustments.
[01:37:33.000 --> 01:37:35.000]  So you get what you get with it.
[01:37:35.000 --> 01:37:45.000]  So the idea for this plot should have been that we look both at the tail on the left and the tail on the right to see if there are effects of common monos.
[01:37:45.000 --> 01:37:53.000]  You don't expect all the strips to fire at the same time or picture to fire at the same time.
[01:37:53.000 --> 01:37:57.000]  You expect to be kind of independent.
[01:37:57.000 --> 01:37:59.000]  Yes.
[01:37:59.000 --> 01:38:06.000]  I mean, one thing we could do, I guess, these 127 limit is on the hybrid, not on the single chips, right?
[01:38:06.000 --> 01:38:19.000]  So in principle, if we will switch on one chip at a time, we could, let's say measure the chip common mode, not clearly the model common mode or the hybrid common mode, but having that really at 50% I guess.
[01:38:19.000 --> 01:38:27.000]  So you still have, you're not going to gain too much because the limit is 32 hits, 32 cluster.
[01:38:27.000 --> 01:38:30.000]  So you gain just a little bit.
[01:38:30.000 --> 01:38:42.000]  You still have to go to 12 clusters, which means instead of 12, instead of eight, I think more or less now you get 12.
[01:38:42.000 --> 01:38:44.000]  So you're not going to gain too much.
[01:38:44.000 --> 01:38:49.000]  You're still pretty far away from having something that is close to 50%.
[01:38:49.000 --> 01:38:52.000]  No, but why? So you say the limit is 32, so you can be...
[01:38:52.000 --> 01:38:54.000]  32.
[01:38:54.000 --> 01:38:55.000]  Second?
[01:38:55.000 --> 01:39:03.000]  It's 32 per the picture chip, and I think it's 24 for the strip chip.
[01:39:03.000 --> 01:39:08.000]  And then overall, you can get up to 127 per hybrid.
[01:39:08.000 --> 01:39:19.000]  No, these are good, but if you enable only one SSA or one MPA at a time, then you get basically 32 out of 128 possible.
[01:39:19.000 --> 01:39:21.000]  Still, you need to...
[01:39:21.000 --> 01:39:23.000]  Sorry, I'm bad.
[01:39:23.000 --> 01:39:32.000]  You still need to stay around half of that because you need to be able to see more hits and less hits.
[01:39:32.000 --> 01:39:37.000]  So it's going to be, you can center up to 16.
[01:39:37.000 --> 01:39:42.000]  So yeah, you're going to get just a factor of two or something like that.
[01:39:42.000 --> 01:39:43.000]  So you don't gain too much.
[01:39:43.000 --> 01:39:48.000]  And also the other reason is that it will take just longer because you have to loop over.
[01:39:48.000 --> 01:39:49.000]  Yeah, that's clear.
[01:39:49.000 --> 01:39:51.000]  Then you need to do it 16 times.
[01:39:51.000 --> 01:39:53.000]  But it's pretty fast right now.
[01:39:53.000 --> 01:39:55.000]  Yeah, it's pretty fast.
[01:39:55.000 --> 01:39:58.000]  It's just going to take basically eight times of that.
[01:39:58.000 --> 01:40:02.000]  So it's just going to sum up a little bit.
[01:40:02.000 --> 01:40:11.000]  The other thing that this came out from the discussion with Giovanni was that in reality is true.
[01:40:11.000 --> 01:40:16.000]  That is nice that you don't expect all the channels to find at the same time.
[01:40:16.000 --> 01:40:20.000]  And also you don't expect all the channels not to fire at the same time.
[01:40:20.000 --> 01:40:31.000]  But at the end, you can also just look at the right tail instead of looking both at the left and the high tail.
[01:40:31.000 --> 01:40:34.000]  And instead of looking both at the low tail and high tail.
[01:40:34.000 --> 01:40:36.000]  So it's just going to give you half of the picture.
[01:40:36.000 --> 01:40:45.000]  But if you're looking for something common in between the two, there is not really something that you can expect that happens just on one of the two side.
[01:40:45.000 --> 01:40:59.000]  So that's why together with the occupancy driven, we have a second plot that is three sigma noise in which you can just look for the tail on one side.
[01:40:59.000 --> 01:41:09.000]  So at the end of the day, what we decided to go for the time being is to have both, same for the two S.
[01:41:09.000 --> 01:41:21.000]  And so we can choose a later point if we see that one of the two is a bit more discriminating.
[01:41:21.000 --> 01:41:27.000]  We can just use one of the two and drop the other or we can just keep them both since it's relatively fast.
[01:41:27.000 --> 01:41:36.000]  And so we can still have a capability to refine our analysis to a later point.
[01:41:36.000 --> 01:41:42.000]  Okay, so I digress a little bit, but just to show you the plots.
[01:41:42.000 --> 01:41:59.000]  So we have one plot at the level of the chip and it's basically this one.
[01:41:59.000 --> 01:42:01.000]  So you see the number of hits.
[01:42:01.000 --> 01:42:09.000]  So the distribution of the events given a certain number of hits, you see that we cannot do something great.
[01:42:09.000 --> 01:42:22.000]  The occupancy is quite low by construction, both for the occupancy driven by even more for the three sigma because the three sigma goes down quite even faster than the occupancy by construction.
[01:42:22.000 --> 01:42:27.000]  And the same is for the NPA.
[01:42:27.000 --> 01:42:29.000]  It's not the same idea.
[01:42:29.000 --> 01:42:35.000]  You see that here we start to see a little bit more tail to the left and right.
[01:42:35.000 --> 01:42:39.000]  And the three sigma and which requires that it look like that.
[01:42:39.000 --> 01:42:43.000]  So we will need to cross check it again.
[01:42:43.000 --> 01:42:45.000]  Yeah, sorry about that.
[01:42:45.000 --> 01:42:50.000]  It's good to go to this plot because I didn't notice that.
[01:42:50.000 --> 01:43:02.000]  And then we have also a few information at the level of the hybrid that gives a little bit more information about the correlation.
[01:43:02.000 --> 01:43:07.000]  Okay, so we have correlation between NPA and SSA.
[01:43:07.000 --> 01:43:14.000]  So number of hits into the SSA and number of hits into the NPA.
[01:43:14.000 --> 01:43:20.000]  Keep forgetting to do this.
[01:43:20.000 --> 01:43:23.000]  Okay, let me zoom in a little more.
[01:43:23.000 --> 01:43:29.000]  And you get a number of events are given a certain number of hits on NPA and SSA.
[01:43:29.000 --> 01:43:33.000]  So you get this distribution.
[01:43:33.000 --> 01:43:37.000]  And you have one for every SSA to NPA couple.
[01:43:37.000 --> 01:43:45.000]  Then we have these that is at the level of the overall hybrid.
[01:43:45.000 --> 01:43:52.000]  So let's keep to the three sigma by mistake.
[01:43:52.000 --> 01:43:53.000]  Is this one?
[01:43:53.000 --> 01:43:55.000]  Okay, same plot with occupancy driven.
[01:43:55.000 --> 01:43:57.000]  The other one was a three sigma.
[01:43:57.000 --> 01:44:08.000]  And you have the same idea number of events in the Z axis and the beans are the number of hits in the strip and number of hits in the pixel.
[01:44:08.000 --> 01:44:15.000]  And then we also get one D plots.
[01:44:15.000 --> 01:44:26.000]  So these are the number of events given a certain number of hits for the pixel and for the strips.
[01:44:26.000 --> 01:44:36.000]  So this was for the occupancy driven exactly the same points are for the three sigma as you see for the pixel is a little bit higher, which is surprising.
[01:44:36.000 --> 01:44:38.000]  We will need to check that.
[01:44:38.000 --> 01:44:41.000]  But for the steps you see that is going quite low.
[01:44:41.000 --> 01:44:49.000]  So the overall idea of this plot, what we're doing the level of potatoes that in the past that was a discussion about doing the feet.
[01:44:49.000 --> 01:45:08.000]  These feet are quite complicated because they assume some some Gaussian behavior of the noise and some, I don't know what's that they evaluate which type of common mode noise they can address.
[01:45:08.000 --> 01:45:13.000]  So the feet wasn't really a good indication what was going on.
[01:45:13.000 --> 01:45:26.000]  We decided just to look for the number of hits above a center number of events above a certain number of which will give us an indication if the module as large for more noise or not.
[01:45:26.000 --> 01:45:32.000]  It will be probably a final little bit more statistic, but for the time being this is the idea.
[01:45:32.000 --> 01:45:44.000]  And we said some reasonable number of hits on which we are cutting and we'll just count on them on many events we have above that.
[01:45:44.000 --> 01:45:58.000]  Okay, so now we have done with the part of the related to basically channel behavior.
[01:45:58.000 --> 01:46:04.000]  Now we're going to go through the electric chain validation.
[01:46:05.000 --> 01:46:20.000]  Do you have any question on what was discussed so far because the picture validation is going to be a bit intense.
[01:46:20.000 --> 01:46:38.000]  Okay, so the goal of the electric chain validation is that we want to so before we saw that we could align the is between themselves and then that we can see meaningful data with the very five steps that we did before.
[01:46:38.000 --> 01:46:49.000]  Now the goal is that we want to see how big is the area in which the module can align, because imagine you have alignment that always works.
[01:46:49.000 --> 01:47:04.000]  But then these alignment has only one possible phase that to choose from these will be potentially bad because once you install into the detector it might be that the conditions that it changes and then the phase is not anymore available.
[01:47:04.000 --> 01:47:10.000]  So the goal of the electric chain validation is to check how wide are these phases.
[01:47:10.000 --> 01:47:20.000]  So we're going to start from so it is a little bit more scramble with respect to the previous one is simply to avoid any extra alignment steps.
[01:47:21.000 --> 01:47:29.000]  So the order is that we start from the electric chain validation between the SSA and the NPA.
[01:47:29.000 --> 01:47:47.000]  So basically, we are checking these the quality of these lines and the overall idea for this plot is basically always sort of a very five step where we inject a pattern and we see how well the patterns are constructed.
[01:47:47.000 --> 01:47:51.000]  But on top of that, we do a scan of phases.
[01:47:51.000 --> 01:47:54.000]  So we force the phase manually.
[01:47:54.000 --> 01:47:59.000]  We don't let the chip, the CIC, the NPA, whatever, choose.
[01:47:59.000 --> 01:48:08.000]  We force it and we also do the test outside the area in which the chip can work.
[01:48:09.000 --> 01:48:20.000]  So starting from the SSA to NPA, these plots are stored into the into the hybrid level.
[01:48:20.000 --> 01:48:30.000]  So since all these are pattern matching, we have two plots, one with the error rate and one with the test base.
[01:48:30.000 --> 01:48:34.000]  You see that we have a few SSA to NPA.
[01:48:34.000 --> 01:48:48.000]  And the reason is that we also scan the current, the drives these lines over here that is set by a register into the SSA.
[01:48:48.000 --> 01:48:58.000]  So we can check with different amount of current that we are injecting this line with the range that we see in NPA because you can imagine that might change a little bit.
[01:48:58.000 --> 01:49:03.000]  So I'm going to open, for example, this one.
[01:49:03.000 --> 01:49:05.000]  Okay.
[01:49:05.000 --> 01:49:07.000]  Okay, forgetting, sorry.
[01:49:07.000 --> 01:49:09.000]  Okay, forgetting.
[01:49:09.000 --> 01:49:16.000]  Don't call Z.
[01:49:16.000 --> 01:49:17.000]  Okay.
[01:49:17.000 --> 01:49:23.000]  So first of all.
[01:49:23.000 --> 01:49:29.000]  So bear with me because I've been more complicated.
[01:49:29.000 --> 01:49:34.000]  With PS, you're going to hear it is a bit more complicated every time.
[01:49:34.000 --> 01:49:49.000]  So in between the SSA and NPA, we don't really have a real phase or actually just two possible phases that sets the edge of the clock on which the NPA is sampling the data coming from the SSA.
[01:49:49.000 --> 01:49:53.000]  So basically, you just have a phase of 50% that you can change.
[01:49:53.000 --> 01:49:58.000]  Now, what does it mean changing the phase?
[01:49:58.000 --> 01:50:08.000]  It potentially also means that you can select the other edge of the clock.
[01:50:08.000 --> 01:50:18.000]  But by doing that, you might go, you might shift by one bit and you can shift it from either to the left or to the right.
[01:50:18.000 --> 01:50:38.000]  So for this reason, in this plot, you're going to see three bins for the falling edge of the clock and three bins for the rising edge of the clock because we also apply an extra offset that might be able to compensate the fact that by changing the clock
[01:50:38.000 --> 01:50:43.000]  edge on which we're sampling, we're moving by one bit to the left or right.
[01:50:43.000 --> 01:50:53.000]  We can honestly reduce the number of bits that are shifting, but I was not sure it was going to sample the first bit before and a bit after.
[01:50:53.000 --> 01:51:00.000]  And that's why for each one of these, I'm going always to do the nominal bit delay, the bit before and the bit after.
[01:51:00.000 --> 01:51:09.000]  Then with experience, we can skim it a little bit and gain a bit of time from this calibration.
[01:51:09.000 --> 01:51:18.000]  So this plot shows the number of tested bits as a function of the different setting that we use.
[01:51:18.000 --> 01:51:22.000]  And for each one of these lines, let me see if I can zoom in.
[01:51:22.000 --> 01:51:29.000]  You see that we have all the lines for all the MPAs.
[01:51:30.000 --> 01:51:32.000]  I can go back.
[01:51:32.000 --> 01:51:33.000]  Okay.
[01:51:33.000 --> 01:51:38.000]  And then on this slide, we have the error rate.
[01:51:38.000 --> 01:51:41.000]  So the plot is the same.
[01:51:41.000 --> 01:51:53.000]  So falling edge, rising edge, and for each one, we shift for plus and minus and no shift at all of the bit that we're using as a first bit.
[01:51:53.000 --> 01:51:59.000]  So what you should expect for a good module is that you have one area.
[01:51:59.000 --> 01:52:00.000]  Okay.
[01:52:00.000 --> 01:52:08.000]  For each one of these lines, at least one area in which you get zero errors.
[01:52:08.000 --> 01:52:22.000]  So you see, for example, that if I take the MPA 14, you see, sorry, 13, you see that there are a few areas for both the two edge or the clock that still works.
[01:52:22.000 --> 01:52:27.000]  So in theory, this MPA can choose both of them.
[01:52:27.000 --> 01:52:32.000]  It doesn't need to be all in the same one because every each one of these setting is independent.
[01:52:32.000 --> 01:52:39.000]  So you can choose independently, but as long as on every horizontal line, there is a working point, you are good to go.
[01:52:39.000 --> 01:52:46.000]  Then as I was saying before, there are a few instabilities that are trying to work it out.
[01:52:46.000 --> 01:52:50.000]  For example, this one is not a problem because you couldn't choose this.
[01:52:50.000 --> 01:52:58.000]  This one is not a problem because we couldn't choose these very likely these are instabilities, but these will pass the test because you still have the possibility to make it work.
[01:52:58.000 --> 01:53:04.000]  This one, you kind of remember, let me open back the original plot.
[01:53:04.000 --> 01:53:09.000]  Those are the same effect that we were seeing over here.
[01:53:09.000 --> 01:53:15.000]  So these lines are the same that we have on these lines.
[01:53:15.000 --> 01:53:18.000]  Okay, so everything is consistent.
[01:53:18.000 --> 01:53:34.000]  So this is telling us that we have good communication and we have at least one working point for each one of these lines, excluding these that have problems between every SSA and every MPA.
[01:53:34.000 --> 01:53:45.000]  So it would be ideal to have two points because that would be really the best, but in reality, when you're turning the clock edge by a full clock cycle, then you're not going to work.
[01:53:45.000 --> 01:53:49.000]  It's quite important to make it work.
[01:53:49.000 --> 01:53:54.000]  Okay.
[01:53:54.000 --> 01:54:03.000]  Then the next step is that we want to check the
[01:54:04.000 --> 01:54:08.000]  phases that we have between the SSA and the SSA.
[01:54:08.000 --> 01:54:14.000]  So every SSA is changing the information of the right and leftmost
[01:54:14.000 --> 01:54:18.000]  strips with the neighboring ones.
[01:54:18.000 --> 01:54:22.000]  So I think I added
[01:54:22.000 --> 01:54:27.000]  something that I got from one of the old people.
[01:54:27.000 --> 01:54:31.000]  So the idea is that you have two nearby SSAs.
[01:54:31.000 --> 01:54:34.000]  So this is one SSA and this is the other one.
[01:54:34.000 --> 01:54:45.000]  And since you can have hits that can go across one SSA and another MPA and you want still to represent the stops, the SSA need to change this information.
[01:54:45.000 --> 01:54:54.000]  So these happens also for the CBC, but for the CBC is different because here, there you have
[01:54:54.000 --> 01:55:02.000]  basically a fake channel connected to the neighbor CBC.
[01:55:02.000 --> 01:55:11.000]  In this case, you really have a line that changes one pattern and the pattern is this one.
[01:55:11.000 --> 01:55:21.000]  So if that line is not properly understood for any reason, then you completely lose the communication between the two MPAs.
[01:55:21.000 --> 01:55:23.000]  So that's what the between the two SSA.
[01:55:23.000 --> 01:55:27.000]  So that's why I want to make sure that we have a good communication between the two.
[01:55:27.000 --> 01:55:32.000]  This is not tested at the level of the quick test because these are only bump ones.
[01:55:32.000 --> 01:55:35.000]  So there is nothing that you can do at that point.
[01:55:35.000 --> 01:55:38.000]  You cannot fix that is not a word born that you can redo.
[01:55:38.000 --> 01:55:44.000]  So that's why we just do it at the level of the full test.
[01:55:44.000 --> 01:55:54.000]  So the plot that we save are kind of similar to what I was showing you now and also the level of the hybrid.
[01:55:54.000 --> 01:56:05.000]  Again, also for this case, we store two plots for every condition because we have the usual error rate and number of test bits.
[01:56:05.000 --> 01:56:13.000]  And also in this case, we can control the current that drives these lines.
[01:56:13.000 --> 01:56:19.000]  So we are also scanning three different currents for these lines.
[01:56:19.000 --> 01:56:31.000]  And as for before, let's see the plots.
[01:56:31.000 --> 01:56:49.000]  Okay, so as for before, what we do is that we show that again, here we don't have a real phase, we just have the selection of the clock edge.
[01:56:49.000 --> 01:56:55.000]  And as for before, we can have a clock shift, a bit shift.
[01:56:55.000 --> 01:57:02.000]  And therefore we are still going to minus one plus zero and plus zero plus one bit shift.
[01:57:02.000 --> 01:57:08.000]  Just to make sure that by shifting the edge, we don't just go into the neighboring bit.
[01:57:08.000 --> 01:57:11.000]  Just to iterate again on this.
[01:57:11.000 --> 01:57:12.000]  It doesn't mean that it's bad.
[01:57:12.000 --> 01:57:18.000]  It means simply that we need, when you change the clock edge, we need also to change the bit.
[01:57:19.000 --> 01:57:26.000]  Okay, and on the y-axis, we show the direction of the communication.
[01:57:26.000 --> 01:57:30.000]  So from which SSA to which SSA.
[01:57:30.000 --> 01:57:38.000]  And you see that we go here, I guess is from left to right and here is from right to left.
[01:57:38.000 --> 01:57:43.000]  And as for the other one, we show the error rate.
[01:57:43.000 --> 01:57:50.000]  So one thing here, you see why we have to do the difference.
[01:57:50.000 --> 01:58:05.000]  You have to check also the one bit before, one bit after, because for this case, when we go from the rising clock to the falling one, you see that some of them, not all of them still work, but they are shifted by one bit.
[01:58:05.000 --> 01:58:07.000]  So that's why we need to do the bit shift.
[01:58:07.000 --> 01:58:14.000]  And for this case, you see that there is at least for each one of these lines, there is a working point.
[01:58:14.000 --> 01:58:16.000]  Honestly, in most of the cases, there are two.
[01:58:16.000 --> 01:58:26.000]  And I guess because the two SSA are so close that it's quite foggy in the communication.
[01:58:26.000 --> 01:58:36.000]  Okay, so these allow you to check between two SSAs, the lateral communication.
[01:58:36.000 --> 01:58:38.000]  Okay.
[01:58:38.000 --> 01:58:44.000]  Then moving onwards, so the CAC to help you with the electric chain validation.
[01:58:44.000 --> 01:58:48.000]  This one is was developed a little bit.
[01:58:48.000 --> 01:58:51.000]  Sometimes advanced with respect to the other.
[01:58:51.000 --> 01:58:57.000]  So just for you, we are checking this communication here.
[01:58:57.000 --> 01:59:01.000]  We are not going to the same order because by changing these, we have to change the alignment.
[01:59:01.000 --> 01:59:03.000]  So we don't want to repeat the alignment to run that.
[01:59:03.000 --> 01:59:11.000]  It's just a technicality to speed up a little bit the steps.
[01:59:11.000 --> 01:59:24.000]  So here, you're going to hear me saying that all the time today, two blocks for every step.
[01:59:24.000 --> 01:59:27.000]  So the number test a bit and error rate.
[01:59:27.000 --> 01:59:33.000]  And here we change a few things at the same time.
[01:59:33.000 --> 01:59:40.000]  So we change the, as for before, we change the amount of current used to drive the lines.
[01:59:40.000 --> 01:59:43.000]  In this case is the CAC that is there in the line.
[01:59:43.000 --> 01:59:45.000]  We can change this.
[01:59:45.000 --> 01:59:48.000]  Then we also change other two parameters.
[01:59:48.000 --> 01:59:59.000]  The clock polarity because it's not shown here, but that PgT provides the clock to the hybrid in particular also to the CAC.
[01:59:59.000 --> 02:00:11.000]  And it is extracted from the, from the, it's a record from the input optical line that is received from the at C7.
[02:00:11.000 --> 02:00:20.000]  And so by changing the clock polarity, you basically change the working point of the CAC and therefore the working, the phases in which this work.
[02:00:20.000 --> 02:00:28.000]  Also, you can set the current that is used to drive the, these clock line.
[02:00:28.000 --> 02:00:36.000]  I think these will be slimmed a little bit because we're scanning a bit too much for what we saw.
[02:00:36.000 --> 02:00:42.000]  But since these are the first modules, we want just to make sure that we have the under control.
[02:00:42.000 --> 02:00:45.000]  So that's why you see a bunch of these plots.
[02:00:45.000 --> 02:00:47.000]  I'm just going to open one.
[02:00:47.000 --> 02:00:53.000]  Hopefully it's going to be a good one.
[02:00:54.000 --> 02:00:57.000]  So the idea is already the same.
[02:00:57.000 --> 02:01:00.000]  Number of test bits and error rate.
[02:01:00.000 --> 02:01:09.000]  So number of test bits here are shown as a function of the LPGVT phase that we're applying the PgVT to sample incoming data.
[02:01:09.000 --> 02:01:18.000]  And these are the seven lines, one for the level one and six for the stops, still these lines.
[02:01:19.000 --> 02:01:23.000]  As usual, stops, stops matching is done into the firmware.
[02:01:23.000 --> 02:01:29.000]  So we can afford to test more bits with respect to the level ones, which are still at 10 to the five.
[02:01:29.000 --> 02:01:31.000]  So it's not a small number.
[02:01:31.000 --> 02:01:45.000]  And then on the error, on the error rate, what you clearly see is that we have a wide area in which we can choose one of these phases, each one of these phases because there were no other.
[02:01:45.000 --> 02:01:52.000]  These two are basically the rising and falling edge of the sigma.
[02:01:52.000 --> 02:02:07.000]  So when you set your clock edge to sample in this area, you don't get a good identification of the bits because the signal is changing at this point.
[02:02:07.000 --> 02:02:09.000]  So you cannot sample here.
[02:02:09.000 --> 02:02:13.000]  But the most important part is that you have plenty of phases that you can choose.
[02:02:13.000 --> 02:02:20.000]  And the LPGVT is you're not choosing one in between, but it could have chosen many more than that.
[02:02:20.000 --> 02:02:23.000]  So this is what will look like a good one.
[02:02:23.000 --> 02:02:29.000]  A bad one, it will mean that you have a small phase range or no phase range at all.
[02:02:29.000 --> 02:02:32.000]  No phase range, you will see it earlier.
[02:02:32.000 --> 02:02:36.000]  Small one, you will see just from this.
[02:02:37.000 --> 02:02:40.000]  Okay. All the other one are just variation of that.
[02:02:40.000 --> 02:02:45.000]  I think you can try to open an inverter clock.
[02:02:45.000 --> 02:02:53.000]  I think this should move things slightly around and not test a bit this one.
[02:02:53.000 --> 02:03:07.000]  You see that things move between the two because you're changing the clock that the CIC is using and therefore is changing the edge that is needed to sample this.
[02:03:07.000 --> 02:03:11.000]  They come in the time to the LPGVT.
[02:03:11.000 --> 02:03:13.000]  Okay.
[02:03:13.000 --> 02:03:20.000]  Then, okay, we go into even more complex territory.
[02:03:21.000 --> 02:03:33.000]  So, you might remember before I was telling you, okay, at this point in time, there is no really way to understand if you have a problem on the stub pattern matching.
[02:03:33.000 --> 02:03:39.000]  Let me open back the plot so you can remember it.
[02:03:39.000 --> 02:03:44.000]  Nope.
[02:03:44.000 --> 02:04:00.000]  So, I was telling you here, we just have one bit, a one bin that tells you there were errors in the stub line, but we cannot really distinguish at this point which of the stub line is causing the issue.
[02:04:00.000 --> 02:04:07.000]  The reason for that is that in order to do that, we need to set the CIC into a bypass mode.
[02:04:07.000 --> 02:04:26.000]  There are a few problems. The first one is that the one is set to the CIC in bypass mode, the phase of these lines changes, and you really need, you need to realign the LPGVT, but you cannot use the automatic alignment because you are not providing the pattern that LPGVT expects, so you need to do a manual scan.
[02:04:26.000 --> 02:04:30.000]  That is what we're going to cover into this step.
[02:04:30.000 --> 02:04:56.000]  And then the other complexity is that the CIC receives an input 48 lines times 8 chip, so 48 lines, but it can output only maximum 7, so you need to do basically few lines at a time.
[02:04:56.000 --> 02:05:15.000]  And by construction, you can do just four lines at a time because the lines are organized into what are called five-port, and when you bypass them, you can bypass one five-port at a time, and these will be sent out to these lines over here.
[02:05:16.000 --> 02:05:36.000]  So you need to loop over all these five-ports, and then it's a bit complicated to do the mapping back, but the PH2SF does it for you because then in reality you would like to see what is the line and the corresponding ID that you use in PH2SF.
[02:05:36.000 --> 02:05:56.000]  So here you have got the full table to see all the mapping, so for every front end we have the trigger lines that are the first nine, yeah, ten five-ports, and the level one are the last two, and then there is an extra complexity, because of course,
[02:05:56.000 --> 02:06:18.000]  the front end ID used by the CIC does not match the I-square CID of the NTAs, that are the ones that we use in PH2SF, and the mapping between the front end ID and the CIC, and the mapping on the front end ID used by the I-square CID and the four by PH2SF is shown here,
[02:06:18.000 --> 02:06:24.000]  and just to be very complicated, most of the thing is different between left and right of this map.
[02:06:25.000 --> 02:06:44.000]  Well, okay, you don't need to know all of that, because the plot that you're going to see already are going to the mapping that you usually use in your XML file and the I-square C map, which are the numbers that you see on the hybrid when you look at the module under the microscope.
[02:06:44.000 --> 02:07:00.000]  Okay, so all of these, so we need to loop over all these five-ports, after we set the NPA to inject a certain pattern, we loop over all of them, and we scan the LPGVT phase in order to find a working point.
[02:07:01.000 --> 02:07:06.000]  So all these plots are stored over here.
[02:07:07.000 --> 02:07:29.000]  They are in this LPGVT for CIC bypass, as usual, bit error rate and test bits, and also we store the best phase, which is the one that we identified with, by looking at the phase range in which you get known.
[02:07:31.000 --> 02:07:33.000]  So let me try to open all of them.
[02:07:34.000 --> 02:07:42.000]  So as usual, number of test bits as a function of the phase that we set on the LPGVT and the lines.
[02:07:42.000 --> 02:07:49.000]  So here the lines are going to always be this, because these are the ones that the lines that are used when you are setting the bypass mode.
[02:07:50.000 --> 02:08:03.000]  And then when you see the error rate, you are going to see there are points in which you have no errors, and we try to set in the center of the largest area with no errors at all.
[02:08:04.000 --> 02:08:11.000]  So these plots, and then we'll get the best phase from this.
[02:08:11.000 --> 02:08:14.000]  So these plots are purely auxiliary.
[02:08:14.000 --> 02:08:26.000]  We store them because when you see problems to a later stage, you want to understand if something was happening, so you can come back and see for any reason. This type was the one thing.
[02:08:27.000 --> 02:08:28.000]  Okay.
[02:08:29.000 --> 02:08:47.000]  So at this point, we identified the best phase given which phyport is sent is bypassed. If I open, for example, another phyport, you're going to see that the phase are different.
[02:08:47.000 --> 02:08:54.000]  So these, unfortunately, need to be repeated for every phase phyport, so that's why I assume it.
[02:08:54.000 --> 02:09:12.000]  But these points are once we identify those phases, then you can safely run the electricity invalidation between the MPAs, in this case, and the CAC, by setting the CAC bypass mode and injecting a part of an MPA.
[02:09:12.000 --> 02:09:18.000]  So we know that this pattern will go through the LGBT, we will have the core phase, and we're going to see it over here.
[02:09:18.000 --> 02:09:27.000]  And then we are able to get the plot from the electricity invalidation between the MPA and the CAC.
[02:09:27.000 --> 02:09:34.000]  Also in this case, we can set the current that drives these lines by setting a register into the MPA.
[02:09:34.000 --> 02:09:37.000]  We do three different settings.
[02:09:37.000 --> 02:09:50.000]  And guess what? We have an error rate and a number of testing bits.
[02:09:50.000 --> 02:09:53.000]  So this one and number of test bits in particular.
[02:09:53.000 --> 02:09:58.000]  So on the x-axis of the phase, on the y-axis, you have the various lines.
[02:09:58.000 --> 02:10:06.000]  You see there are two empty columns because these two phases don't work into the CAC.
[02:10:06.000 --> 02:10:12.000]  And for the bit error rate, this honestly was quite a good plot.
[02:10:12.000 --> 02:10:26.000]  It's not always this good, but you see that for every of these lines, you get a very wide area in which any one of these phases will work because it doesn't cause any.
[02:10:26.000 --> 02:10:32.000]  And this concludes the electric chain validation.
[02:10:32.000 --> 02:10:38.000]  I know that is quite heavy, but I think the plots are a little bit more clear once they're explained.
[02:10:38.000 --> 02:10:50.000]  So the whole idea is simply check how wide is the phase space in which you can find a phase that will allow you to properly set the communication between two phases.
[02:10:50.000 --> 02:11:01.000]  And the various steps are just to check different lines that are in between two phases.
[02:11:01.000 --> 02:11:14.000]  I'm just going to move forward, but if I have any questions, just ask and stop me anytime.
[02:11:14.000 --> 02:11:19.000]  Okay, so we're almost at the end.
[02:11:19.000 --> 02:11:21.000]  Bit error rate test.
[02:11:21.000 --> 02:11:34.000]  So this uses a functionality of the LPGVT in which we are able to inject pseudo random pattern that we refer to as PRBS.
[02:11:34.000 --> 02:11:44.000]  And these particular tests is done by pretending that the PRBS is generated by these lines.
[02:11:44.000 --> 02:11:52.000]  And then these send all the way back to the FPGA, split back into the separated components of the line.
[02:11:52.000 --> 02:12:02.000]  And then we check if the generated patterns matches the received pattern.
[02:12:02.000 --> 02:12:11.000]  So we know what exactly the sequences so we can set the same sequence in the FPGA and we do this match.
[02:12:11.000 --> 02:12:20.000]  So just one technical thing, we are technically just testing one line, but you're going to see plot for the different lines.
[02:12:20.000 --> 02:12:29.000]  And this is simply due to the fact that we don't have enough resources to have all in the firmware to have all the lines done in one shot.
[02:12:29.000 --> 02:12:32.000]  So we are going to do one line at a time.
[02:12:32.000 --> 02:12:37.000]  And in theory, we can just sum all the results together.
[02:12:37.000 --> 02:12:50.000]  But since it's quite new, we prefer to keep them separated so we can distinguish from some sort of behavior that might be related to some bug or something like that.
[02:12:50.000 --> 02:12:56.000]  So but from point of view of analysis, you can just assume to sum them all together.
[02:12:56.000 --> 02:13:03.000]  Okay, wrong knot.
[02:13:03.000 --> 02:13:12.000]  So these are stored at the level of the optical group.
[02:13:12.000 --> 02:13:17.000]  And we store a few plots.
[02:13:17.000 --> 02:13:33.000]  So the first one that we do is a face scan because these pseudo random pattern are generated into the LPGVT using a cloud source.
[02:13:33.000 --> 02:13:43.000]  But the LPGVT still need to properly decode this pattern in order to shoot them out correctly.
[02:13:43.000 --> 02:13:52.000]  And therefore, you have the possibility to change the clock phases such that the LPGVT quickly samples those data.
[02:13:52.000 --> 02:14:04.000]  And therefore, the first step is to do this clock face scan in which for each one of these lines, we identify the working point of the clock.
[02:14:05.000 --> 02:14:10.000]  Basically, we need to find a wide area in which things are working fine.
[02:14:10.000 --> 02:14:13.000]  And the best phase is then stored over here.
[02:14:13.000 --> 02:14:19.000]  So this is just a technicality to make things work and on creating spurious errors.
[02:14:19.000 --> 02:14:35.000]  Just a small caveat. So I did this measurement with a 10G module and with a 10G in this case, the clock face span is fixed.
[02:14:35.000 --> 02:14:41.000]  It doesn't change if we go 10 giga or 5 giga.
[02:14:41.000 --> 02:14:44.000]  And that's why I don't need to do the full scan.
[02:14:44.000 --> 02:14:46.000]  I just need to do half of the scan.
[02:14:46.000 --> 02:14:49.000]  So that's why you are never going to see anything on top of that.
[02:14:49.000 --> 02:14:54.000]  Just to prove you that I'm not doing it at all for all the other ones.
[02:14:54.000 --> 02:14:56.000]  We store also the number of tests a bit.
[02:14:56.000 --> 02:15:00.000]  Let me see that here. We don't do absolutely anything.
[02:15:00.000 --> 02:15:10.000]  But once we identify the correct phase with this scan, then we can actually do the real bit error rate test.
[02:15:10.000 --> 02:15:18.000]  And as usual, we store two information, the number of tests a bit that you see.
[02:15:18.000 --> 02:15:28.000]  So we are collecting 10 to the 10, kind of 10 to the 10 bits for every line.
[02:15:28.000 --> 02:15:30.000]  And as I was saying, you can sum them all together.
[02:15:30.000 --> 02:15:35.000]  It's a separate acquisition, but just they're testing the same thing.
[02:15:36.000 --> 02:15:43.000]  And so we are doing roughly 10 to the 11 bits that are tested.
[02:15:43.000 --> 02:15:48.000]  You also see there is some symmetry simply because we do that the hybrid's in parallel.
[02:15:48.000 --> 02:15:52.000]  So we stop these, but one line at a time.
[02:15:52.000 --> 02:15:56.000]  So we stop these two hybrids at the same time, these two hybrid at the same time.
[02:15:56.000 --> 02:16:04.000]  So that's why the beta tests that are kind of symmetric because we are doing the two acquisition in parallel.
[02:16:05.000 --> 02:16:11.000]  And you see, we don't really have a way to ask a number of bits.
[02:16:11.000 --> 02:16:14.000]  We just calculate how long it's going to take.
[02:16:14.000 --> 02:16:24.000]  And we just check that the amount of bits that are tested matches that are at least as big as the number of bits.
[02:16:24.000 --> 02:16:28.000]  And then the bit error rate shows something like that.
[02:16:28.000 --> 02:16:30.000]  It should be over zero.
[02:16:30.000 --> 02:16:38.000]  So for this type of application, the request is usually to be below 10 to the minus 12, 10 to the minus 13.
[02:16:38.000 --> 02:16:43.000]  And since you are testing 10 to the 11, you will expect always zero.
[02:16:43.000 --> 02:16:55.000]  The reason why we cannot really do in production more than that is because this is really the time that you need to wait for receiving all these bits.
[02:16:55.000 --> 02:17:02.000]  And if you had to do around 10 to the 13, it comes to be around a few hours of data taking.
[02:17:02.000 --> 02:17:04.000]  So it is not really feasible.
[02:17:04.000 --> 02:17:07.000]  We did it on a couple of modules while developing it.
[02:17:07.000 --> 02:17:15.000]  And we will probably do it on more modules in the future just to have sort of an idea of things that are behaving overall.
[02:17:15.000 --> 02:17:23.000]  But on every modules, it becomes very complicated because it will take quite a lot of time.
[02:17:24.000 --> 02:17:33.000]  Okay. And then the last plot that we save at the same time of the bit error rate test is the forward error correction counter.
[02:17:33.000 --> 02:17:39.000]  So these modules implement a FAC5.
[02:17:39.000 --> 02:17:43.000]  They stand for forward error correction five bits.
[02:17:43.000 --> 02:17:52.000]  And the reason is that they have a special extra bits that allow to correct up to five bit flips.
[02:17:53.000 --> 02:18:04.000]  So the thing that we want to check is that not only you don't have a bit error rate test, but you also don't have error that were there, but they were actually corrected.
[02:18:04.000 --> 02:18:09.000]  So in the firmware, we had the possibility to count how many errors were corrected.
[02:18:09.000 --> 02:18:17.000]  And in this case, you don't see it anymore split for the two left and right hybrid because this is a property of the overall packet.
[02:18:17.000 --> 02:18:26.000]  So we just count how many errors were in the packet that contains both the left and right hybrid.
[02:18:26.000 --> 02:18:28.000]  So this is a single plot.
[02:18:28.000 --> 02:18:41.000]  So what might happen is that you see FAC counters, but you don't see any bit error counters simply because the errors were, we were able to correct them, which is good.
[02:18:41.000 --> 02:18:51.000]  But we also want to make sure that we don't have any error to correct because the communication is good to start with.
[02:18:51.000 --> 02:18:59.000]  Okay, and now I'm really at the end of the result file and it's the OT register tester.
[02:18:59.000 --> 02:19:02.000]  So this is a very simple test.
[02:19:02.000 --> 02:19:15.000]  So what we do is that we read and write, I think, 1000 times a few registers in all the ASAPs that we have at the level of the hybrid of the SQLC.
[02:19:15.000 --> 02:19:18.000]  So this is a check of the SQLC stability.
[02:19:18.000 --> 02:19:31.000]  And with a store in a plot that is at the level of the hybrid, what the efficiency for each of the chips for you see the SSA, the MPA and the SAC.
[02:19:31.000 --> 02:19:40.000]  Here you want to basically check if you have any problem basically in the connector between the front end and the without hybrid.
[02:19:41.000 --> 02:19:46.000]  Because if you have a bad connection, you might have some stability in this process.
[02:19:46.000 --> 02:19:48.000]  So far, we never seen anything like that.
[02:19:48.000 --> 02:19:53.000]  And efficiency is always 100% because that process is very stable.
[02:19:53.000 --> 02:19:58.000]  It should be very stable for all the good models.
[02:19:58.000 --> 02:20:14.000]  Okay, so this was the overall discussion for the plots that are produced by the full test and these include also the quick test.
[02:20:14.000 --> 02:20:19.000]  So I'm going to next move to the monitor BQM file.
[02:20:19.000 --> 02:20:30.000]  So I think it's a good time to stop to see if you have any comments or question what was discussed today.
[02:20:30.000 --> 02:20:37.000]  Until then.
[02:20:37.000 --> 02:20:39.000]  Okay.
[02:20:40.000 --> 02:20:42.000]  Then monitor BQM plot.
[02:20:42.000 --> 02:20:55.000]  So all these information that we're storing are contained into the XML file just for you one as a reference.
[02:20:55.000 --> 02:20:58.000]  They are located over here.
[02:20:58.000 --> 02:21:04.000]  At the end of the XML file and it contains the list of information that we are storing.
[02:21:04.000 --> 02:21:13.000]  For the PS we're storing both information read by the LPGT and information read by the SSA and the MPA.
[02:21:13.000 --> 02:21:16.000]  So I'm going to go to all of these plots.
[02:21:16.000 --> 02:21:28.000]  So these are T-graph and as I was saying are stored into a separate plot such that you can monitor also when you are not in a running condition.
[02:21:28.000 --> 02:21:34.000]  So starting, just closing everything.
[02:21:34.000 --> 02:21:41.000]  So the first, so it's going to look like the same structure of the Resalphi.
[02:21:41.000 --> 02:21:50.000]  And then at the optical group, we start to see the monitor of the LPGT.
[02:21:50.000 --> 02:21:57.000]  So these are in particular first three are measurement of some values that are read inside the LPGT.
[02:21:57.000 --> 02:22:01.000]  And in particular, this one is one voltage.
[02:22:01.000 --> 02:22:05.000]  They call VDD that should be around 1.2 volts.
[02:22:05.000 --> 02:22:07.000]  It's more or less stable.
[02:22:07.000 --> 02:22:12.000]  Just please ignore if you see some large swing like that.
[02:22:12.000 --> 02:22:16.000]  The ADC readout sometimes fails.
[02:22:16.000 --> 02:22:18.000]  So you might see some sort of swing.
[02:22:18.000 --> 02:22:21.000]  So these are clearly not real.
[02:22:21.000 --> 02:22:23.000]  So don't be alarmed when you have something like that.
[02:22:23.000 --> 02:22:30.000]  These are the beginning will sometimes happen at any time.
[02:22:30.000 --> 02:22:33.000]  And the same for these other voltage.
[02:22:33.000 --> 02:22:37.000]  I honestly, not 100% sure what are these values.
[02:22:37.000 --> 02:22:39.000]  So why we have these two values?
[02:22:39.000 --> 02:22:48.000]  I would guess there are two blocks that have two different voltages to avoid first talk or something like that in basic.
[02:22:48.000 --> 02:22:49.000]  Those are available.
[02:22:49.000 --> 02:22:51.000]  We monitor them both.
[02:22:51.000 --> 02:22:53.000]  And they are both more or less to the same value.
[02:22:53.000 --> 02:22:57.000]  So far we haven't seen surprises.
[02:22:57.000 --> 02:23:01.000]  Then the temperature.
[02:23:01.000 --> 02:23:05.000]  So you see here there are some swings that we can sort of ignore.
[02:23:05.000 --> 02:23:12.000]  That means you mean this is the temperature of the sensor that is within the LPGT.
[02:23:12.000 --> 02:23:17.000]  This should be quite accurate because we already have the calibration of all the LPGT.
[02:23:17.000 --> 02:23:21.000]  So this value should be quite realistic.
[02:23:21.000 --> 02:23:35.000]  I don't know exactly in terms of degrees because I don't remember what the LPGT group said, but it should be the absolute value should be quite realistic.
[02:23:35.000 --> 02:23:43.000]  Just open to the flag.
[02:23:43.000 --> 02:23:59.000]  And the other information are read out by input that are provided to the LPGT via some connector of the data that then go into the internal ADC of the LPGT.
[02:23:59.000 --> 02:24:03.000]  The first one is the sensor temperature.
[02:24:03.000 --> 02:24:12.000]  And with the sensor temperature, I mean the one that sticks out from the ROH and is in contact with the sensor.
[02:24:12.000 --> 02:24:20.000]  So this is basically equivalent of what we have for the 2S.
[02:24:20.000 --> 02:24:38.000]  So also this one, the measurement of the temperature of the resistor should be quite accurate because we have the calibration of the LPGT as well as the nominal NTC calibration.
[02:24:38.000 --> 02:24:44.000]  This is the NCT, so it is the name of the resistor.
[02:24:44.000 --> 02:25:02.000]  So then there is the question of how accurate is the measurement of the sensor temperature because of not great contact with the sensor itself and some power that is leaked from the ROH.
[02:25:02.000 --> 02:25:11.000]  But the measurement should be quite accurate to the temperature that is seen by the resistor.
[02:25:11.000 --> 02:25:26.000]  Then we have the measurement of the leakage current on the diode that converts the optical signal into an electrical signal.
[02:25:26.000 --> 02:25:41.000]  As far as I understood, this is the measurement basically of the, so it's going to be used during data taking to monitor the radiation damage of this diode.
[02:25:41.000 --> 02:25:51.000]  This we have to have an idea of the yield of the signal that is collected by the diode.
[02:25:51.000 --> 02:25:58.000]  So this should be the average current generated by the light that is being received.
[02:25:58.000 --> 02:26:02.000]  So for the time being, we don't really have a plan to use it.
[02:26:02.000 --> 02:26:17.000]  I would say that if we have problems with this diode, we will see this in effect from other behavior of the plot.
[02:26:17.000 --> 02:26:22.000]  Sorry, now I have also to open to the dog.
[02:26:22.000 --> 02:26:28.000]  Okay, and then we also monitor for the VTRX the temperature.
[02:26:28.000 --> 02:26:34.000]  And as you can see already without an zoom, we are having some issue over here.
[02:26:34.000 --> 02:27:00.000]  So, first of all, this is the temperature that is a temperature sensor that we read from the VTRX itself is not going to be super accurate in terms of absolute value because we don't have a calibration value for that.
[02:27:00.000 --> 02:27:09.000]  But as all the temperature sensor that we have, those are quite released in terms of relative variations.
[02:27:09.000 --> 02:27:29.000]  Now, while we have these, okay, this was due to a recent thing that was added and we actually, Stefan found out that we were setting the, so you need to set up a current to be provided to the resistors.
[02:27:29.000 --> 02:27:41.000]  In order to measure the temperature and what we were doing is that we were setting the current given the expected resistors in order to have a good resolution.
[02:27:41.000 --> 02:27:51.000]  The problem with that is that when you go down in temperature, the value of the resistor changes drastically by the factor of 10 or even more.
[02:27:51.000 --> 02:27:55.000]  And therefore, if you use the nominal value of room temperature will not work anymore.
[02:27:55.000 --> 02:28:06.000]  So what was added is that instead of using the previous values, but then when you have that the readout of the ADC fails once that sometimes happened, then it got stuck.
[02:28:06.000 --> 02:28:16.000]  So I tried to fix it a little bit better and it works as we've seen for the sensor temperature before the VTRX is still not working fine.
[02:28:16.000 --> 02:28:21.000]  So that's why you see this ring, but we're addressing that.
[02:28:22.000 --> 02:28:32.000]  And finally, with the PS, we also monitor the LPG, the 2.5 volts.
[02:28:32.000 --> 02:28:39.000]  That is the one that is applied to the LPGVT server to the VTRX that requires 2.5 volts.
[02:28:39.000 --> 02:28:43.000]  So here we can monitor the voltage.
[02:28:43.000 --> 02:28:50.000]  Okay, so these are the information that we store at the level of the LPGVT.
[02:28:50.000 --> 02:28:55.000]  And then the next information that we can store for the SSN and PA.
[02:28:55.000 --> 02:29:10.000]  So we have two voltages, the analog voltage that is around 1.2, 1.3.
[02:29:10.000 --> 02:29:18.000]  And as well as the digital voltage that it should be around 1 is slightly higher than 1.
[02:29:18.000 --> 02:29:23.000]  So here, so far, I don't think we saw anything, but it might be an indication.
[02:29:23.000 --> 02:29:27.000]  Maybe if a certain amount something fails, you can see a drastic drop or something like that.
[02:29:27.000 --> 02:29:32.000]  So I would guess we'll be using failure cases to do a bit of debugging.
[02:29:32.000 --> 02:29:40.000]  But so far, if you don't have enough power, you will see clearly some other problems.
[02:29:40.000 --> 02:29:43.000]  And finally, we have the temperature measurement.
[02:29:43.000 --> 02:29:46.000]  This was briefly discussed at the beginning.
[02:29:46.000 --> 02:29:49.000]  This is the part that at the moment we still don't have the calibration.
[02:29:49.000 --> 02:29:56.000]  So the relative variation should be quite accurate.
[02:29:56.000 --> 02:30:04.000]  But the absolute value is not.
[02:30:04.000 --> 02:30:13.000]  So this was probably higher than what we're showing here because we were running long calibration without cooling.
[02:30:13.000 --> 02:30:22.000]  So these don't use them as absolute values until we get the real calibration.
[02:30:22.000 --> 02:30:25.000]  And for the NPA is the same.
[02:30:25.000 --> 02:30:34.000]  We store the two measurements of the digital and analog current voltage as well as the temperature.
[02:30:34.000 --> 02:30:46.000]  And as for the SSA, also this is not really accurate in terms of absolute value.
[02:30:46.000 --> 02:30:59.000]  And those are all the parameters that were currently monitored during all the measurements that we do with the PH2SEL.
[02:30:59.000 --> 02:31:03.000]  Any questions or comments on this?
[02:31:03.000 --> 02:31:12.000]  Okay.
[02:31:12.000 --> 02:31:24.000]  So we are at the end of this discussion.
[02:31:24.000 --> 02:31:50.000]  So I think we can, so I'm going to just ask you if there is anything that you would like to better understand what was discussed today.
[02:31:50.000 --> 02:31:55.000]  Sorry, I have a question.
[02:31:55.000 --> 02:31:57.000]  Maybe I missed something.
[02:31:57.000 --> 02:32:03.000]  At some point you said that the first and the last three are duplicated.
[02:32:03.000 --> 02:32:07.000]  There's a lot of slides so I don't remember exactly where it was.
[02:32:07.000 --> 02:32:11.000]  But I didn't really understand what was the reason.
[02:32:11.000 --> 02:32:26.000]  Yeah, it's simply because when you have, I'm going to open in the meantime, when you have to do the bombarding of the two asyps,
[02:32:26.000 --> 02:32:34.000]  it is quite difficult to have a very small space in between the two chips.
[02:32:34.000 --> 02:32:42.000]  So what they usually do, and this is done also for the inner tracker for the current and I think also the future.
[02:32:42.000 --> 02:33:00.000]  What they do is that they make the picture at the edge between the two chips a bit wider such that you have a bit more room to compensate for the fact that you cannot cut the basic super precise.
[02:33:00.000 --> 02:33:03.000]  So this is usually the case.
[02:33:03.000 --> 02:33:09.000]  In our case, there is an extra complication that you need to reconstruct the steps.
[02:33:09.000 --> 02:33:16.000]  So you need to have the capability to match the pixel with the strips.
[02:33:16.000 --> 02:33:30.000]  And since you have 120 columns in the strips,
[02:33:30.000 --> 02:33:37.000]  it is much easier if you can do 120 pixel 2.
[02:33:37.000 --> 02:33:49.000]  Simply because then you don't need to think, okay, I have to shift one strip by one because I need to match with the picture that is actually shifted.
[02:33:49.000 --> 02:34:02.000]  It's just a small trick in which they just duplicate everything they pretend that the pixel are actually 16 by 120.
[02:34:02.000 --> 02:34:15.000]  And then you just feed everything to the cluster mechanism that reconstruct the clusters that that point is completely agnostic about what is going on before just going to receive every time a two hits from the corner pixel and that's it.
[02:34:15.000 --> 02:34:19.000]  And then we'll assume that that is a pixel of cluster size too.
[02:34:19.000 --> 02:34:23.000]  And it will be much easier to match it with the strips.
[02:34:23.000 --> 02:34:34.000]  As you saw in practice actually the pixels are actually 118 on the side.
[02:34:34.000 --> 02:34:52.000]  Yes, exactly 118 on the sensor 118 and on the pixel are 120 but the two at the edge are fake are just duplicating whatever they see in the in the previous picture.
[02:34:52.000 --> 02:34:54.000]  I see. Okay, thank you.
[02:34:54.000 --> 02:34:56.000]  No problem.
[02:35:05.000 --> 02:35:18.000]  Okay, anything else that you want to chat about.
[02:35:18.000 --> 02:35:26.000]  Okay, then what I'm gonna do is that I'm gonna stop the recording.
[02:35:26.000 --> 02:35:29.000]  I'm sharing.
