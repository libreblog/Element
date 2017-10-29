
#include "controllers/AppController.h"
#include "controllers/EngineController.h"
#include "controllers/GuiController.h"
#include "controllers/SessionController.h"
#include "engine/GraphProcessor.h"
#include "gui/UnlockForm.h"
#include "gui/GuiCommon.h"
#include "session/UnlockStatus.h"
#include "Commands.h"
#include "Globals.h"
#include "Messages.h"
#include "Settings.h"
#include "Version.h"

namespace Element {

static void showProductLockedAlert (const String& msg = String(), const String& title = "Feature not Available")
{
    String message = (msg.isEmpty()) ? "Unlock the full version of Element to use this feature.\nGet a copy @ https://kushview.net"
                                     : msg;
    if (AlertWindow::showOkCancelBox (AlertWindow::InfoIcon, title, message, "Upgrade", "Cancel"))
        URL("https://kushview.net/products/element/").launchInDefaultBrowser();
}

    Globals& AppController::Child::getWorld()
    {
        auto* app = dynamic_cast<AppController*> (getRoot());
        jassert(app);
        return app->getWorld();
    }
    
AppController::AppController (Globals& g)
    : world (g)
{
    addChild (new GuiController (g, *this));
    addChild (new EngineController ());
    addChild (new SessionController ());
    g.getCommandManager().registerAllCommandsForTarget (this);
    g.getCommandManager().setFirstCommandTarget (this);
}

AppController::~AppController() { }

void AppController::activate()
{
    Controller::activate();
}

void AppController::deactivate()
{
    Controller::deactivate();
}

void AppController::run()
{
    activate();
    auto session = getWorld().getSession();

    
    if (auto* gui = findChild<GuiController>())
        gui->run();
    
    if (auto* sc = findChild<SessionController>())
    {
        const auto lastSession = getWorld().getSettings().getUserSettings()->getValue ("lastSession");
        if (File::isAbsolutePath (lastSession))
            sc->openFile (File (lastSession));
    }
    
    if (auto* gui = findChild<GuiController>())
    {
        const Node graph (session->getCurrentGraph());
        gui->stabilizeContent();
        if (graph.isValid())
        {
            for (int i = graph.getNumNodes(); --i >= 0;)
                if ((bool) graph.getNode(i).getProperty ("windowVisible", false))
                    ViewHelpers::presentPluginWindow (graph.getNode (i));
        }
    }
    
    if (auto* sc = findChild<SessionController>())
        sc->resetChanges();
}

void AppController::handleMessage (const Message& msg)
{
	auto* ec = findChild<EngineController>();
	jassert(ec);

    bool handled = true;

    if (const auto* lpm = dynamic_cast<const LoadPluginMessage*> (&msg))
    {
        ec->addPlugin (lpm->description, lpm->relativeX, lpm->relativeY);
    }
    else if (const auto* rnm = dynamic_cast<const RemoveNodeMessage*> (&msg))
    {
        ec->removeNode (rnm->nodeId);
    }
    else if (const auto* acm = dynamic_cast<const AddConnectionMessage*> (&msg))
    {
        if (acm->useChannels())
            ec->connectChannels (acm->sourceNode, acm->sourceChannel, acm->destNode, acm->destChannel);
        else
            ec->addConnection (acm->sourceNode, acm->sourcePort, acm->destNode, acm->destPort);
    }
    else if (const auto* rcm = dynamic_cast<const RemoveConnectionMessage*> (&msg))
    {
        if (rcm->useChannels())
		{
            jassertfalse; // channels not yet supported
        }
        else
        {
            ec->removeConnection (rcm->sourceNode, rcm->sourcePort, rcm->destNode, rcm->destPort);
        }
    }
    else if (const auto* dnm = dynamic_cast<const DuplicateNodeMessage*> (&msg))
    {
        ValueTree parent (dnm->node.getValueTree().getParent());
        if (parent.hasType (Tags::nodes))
            parent = parent.getParent();
        jassert (parent.hasType(Tags::node));
        const Node graph (parent, false);
        Node newNode (dnm->node.getValueTree().createCopy(), false);
        if (newNode.isValid() && graph.isValid())
        {
            newNode.getValueTree().removeProperty (Tags::id, 0);
            ec->addNode (newNode);
        }
    }
    else if (const auto* dnm2 = dynamic_cast<const DisconnectNodeMessage*> (&msg))
    {
        ec->disconnectNode (dnm2->node);
    }
    else
    {
        handled = false;
    }
    
    if (! handled)
    {
        DBG("[EL] AppController: unhandled Message received");
    }
}


ApplicationCommandTarget* AppController::getNextCommandTarget()
{
    return findChild<GuiController>();
}

void AppController::getAllCommands (Array<CommandID>& cids)
{
    cids.addArray ({
        Commands::mediaNew,
        Commands::mediaOpen,
        Commands::mediaSave,
        Commands::mediaSaveAs,
        
        Commands::signIn,
        Commands::signOut,
        
        Commands::sessionNew,
        Commands::sessionSave,
        Commands::sessionSaveAs,
        Commands::sessionOpen,
        Commands::sessionAddGraph,
        Commands::sessionDuplicateGraph,
        Commands::sessionDeleteGraph,
        Commands::sessionInsertPlugin,
        
        Commands::importGraph,
        Commands::exportGraph,
        Commands::panic,
        
        Commands::checkNewerVersion
    });
    cids.addArray({ Commands::copy, Commands::paste });
}

void AppController::getCommandInfo (CommandID commandID, ApplicationCommandInfo& result)
{
    Commands::getCommandInfo (commandID, result);
}

bool AppController::perform (const InvocationInfo& info)
{
    auto& status (world.getUnlockStatus());
	auto& settings(getGlobals().getSettings());
    bool res = true;
    switch (info.commandID)
    {
        case Commands::sessionOpen:
        {
            FileChooser chooser ("Open Session", lastSavedFile, "*.els", true, false);
            if (chooser.browseForFileToOpen())
                findChild<SessionController>()->openFile (chooser.getResult());
        } break;
        case Commands::sessionNew:
            findChild<SessionController>()->newSession();
            break;
        case Commands::sessionSave:
            findChild<SessionController>()->saveSession (false);
            break;
        case Commands::sessionSaveAs:
            findChild<SessionController>()->saveSession (true);
            break;
        case Commands::sessionClose:
            findChild<SessionController>()->closeSession();
            break;
        case Commands::sessionAddGraph:
            findChild<EngineController>()->addGraph();
            break;
        case Commands::sessionDuplicateGraph:
            findChild<EngineController>()->duplicateGraph();
            break;
        case Commands::sessionDeleteGraph:
            findChild<EngineController>()->removeGraph();
            break;
        
        case Commands::importGraph:
        {
            FileChooser chooser ("Import Graph", lastSavedFile, "*.elg");
            if (world.getUnlockStatus().isFullVersion() && chooser.browseForFileToOpen())
                findChild<SessionController>()->importGraph (chooser.getResult());
            else
                Element::showProductLockedAlert();
        } break;
            
        case Commands::exportGraph:
        {
            FileChooser chooser ("Export Graph", lastSavedFile, "*.elg");
            if (world.getUnlockStatus().isFullVersion() && chooser.browseForFileToSave (true))
                findChild<SessionController>()->exportGraph (world.getSession()->getCurrentGraph(),
                                                             chooser.getResult());
            else
                Element::showProductLockedAlert();
        } break;

        case Commands::panic:
        {
            auto e = getWorld().getAudioEngine();
            for (int c = 1; c <= 16; ++c)
            {
                auto msg = MidiMessage::allNotesOff (c);
                msg.setTimeStamp (Time::getMillisecondCounterHiRes());
                e->addMidiMessage (msg);
                msg = MidiMessage::allSoundOff(c);
                msg.setTimeStamp (Time::getMillisecondCounterHiRes());
                e->addMidiMessage (msg);
            }
        }  break;
            
        case Commands::mediaNew:
        case Commands::mediaSave:
        case Commands::mediaSaveAs:
            break;
        
        case Commands::signIn:
        {
            auto* form = new UnlockForm (status, "Enter your license key.",
                                         false, false, true, true);
            DialogWindow::LaunchOptions opts;
            opts.content.setOwned (form);
            opts.resizable = false;
            opts.dialogTitle = "License Manager";
            opts.runModal();
        } break;
        
        case Commands::signOut:
        {
            if (status.isUnlocked())
            {
                auto* props = settings.getUserSettings();
                props->removeValue("L");
                props->save();
                status.load();
            }
        } break;
        
        case Commands::checkNewerVersion:
            CurrentVersion::checkAfterDelay (20, true);
            break;
        
        default: res = false; break;
    }
    return res;
}

}
