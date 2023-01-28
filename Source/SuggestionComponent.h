/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include "PluginEditor.h"

// Component that sits on top of a TextEditor and will draw auto-complete suggestions over it
class AutoCompleteComponent
    : public Component
    , public ComponentListener
{
    String suggestion;
    Canvas* cnv;
    Component::SafePointer<TextEditor> editor;
        
public:
    
    AutoCompleteComponent(TextEditor* e, Canvas* c) : editor(e), cnv(c)
    {
        setAlwaysOnTop(true);
        
        editor->addComponentListener(this);
        cnv->addAndMakeVisible(this);
        
        setInterceptsMouseClicks(false, false);
    }
    
    ~AutoCompleteComponent() {
        editor->removeComponentListener(this);
    }
    
    String getSuggestion() {
        
        if(!editor) return String();
        
        return editor->getText() + suggestion;
    }
    
    void autocomplete() {
        if(!editor) return;
        
        editor->setText(editor->getText() + suggestion, sendNotification);
    }
    
    void setSuggestion(const String& suggestionText) {
        if(!editor) return;
        
        auto textUpToSpace = editor->getText().upToFirstOccurrenceOf(" ", false, false);
        
        setVisible(suggestionText.isNotEmpty() && textUpToSpace != suggestionText);
        
        suggestion = suggestionText.fromFirstOccurrenceOf(textUpToSpace, false, true);
        repaint();
    }
    
private:
    void componentMovedOrResized(Component& component, bool moved, bool resized) override
    {
        if(!editor) return;
        setBounds(cnv->getLocalArea(editor, editor->getLocalBounds()));
    }
    

    void paint(Graphics& g) override
    {
        if(!editor) return;
        
        auto editorText = editor->getText();
        auto editorTextWidth = editor->getFont().getStringWidthFloat(editorText);
        auto completionBounds = getLocalBounds().toFloat().withTrimmedLeft(editorTextWidth + 7.5f);
        
        auto colour = findColour(PlugDataColour::canvasTextColourId).withAlpha(0.65f);
        PlugDataLook::drawText(g, suggestion, completionBounds, colour);
    }
};
// Suggestions component that shows up when objects are edited
class SuggestionComponent : public Component
    , public KeyListener
    , public TextEditor::Listener {

    class Suggestion : public TextButton {
        int idx = 0;
        int type = -1;

        String objectDescription;

    public:
        Suggestion(SuggestionComponent* parentComponent, int i)
            : idx(i)
            , parent(parentComponent)
        {
            setText("", "", false);
            setWantsKeyboardFocus(false);
            setConnectedEdges(12);
            setClickingTogglesState(true);
            setRadioGroupId(1001);
            setColour(TextButton::buttonOnColourId, findColour(ScrollBar::thumbColourId));
        }

        void setText(String const& name, String const& description, bool icon)
        {
            objectDescription = description;
            setButtonText(name);
            type = name.contains("~") ? 1 : 0;

            // Argument suggestions don't have icons!
            drawIcon = icon;

            repaint();
        }

        // TODO: why is this necessary?
        void mouseDown(MouseEvent const& e) override
        {
            onClick();
        }

        void paint(Graphics& g) override
        {
            auto scrollbarIndent = parent->port->canScrollVertically() ? 5 : 0;

            auto backgroundColour = findColour(getToggleState() ? PlugDataColour::popupMenuActiveBackgroundColourId : PlugDataColour::popupMenuBackgroundColourId);

            auto buttonArea = getLocalBounds().reduced(6, 2).withTrimmedRight(scrollbarIndent).toFloat();

            g.setColour(backgroundColour);
            g.fillRoundedRectangle(buttonArea, PlugDataLook::defaultCornerRadius);

            auto colour = getToggleState() ? findColour(PlugDataColour::popupMenuActiveTextColourId) : findColour(PlugDataColour::popupMenuTextColourId);

            auto yIndent = jmin(4, proportionOfHeight(0.3f));
            auto leftIndent = drawIcon ? 34 : 11;
            auto rightIndent = 11;
            auto textWidth = getWidth() - leftIndent - rightIndent;

            if (textWidth > 0)
                PlugDataLook::drawStyledText(g, getButtonText(), leftIndent, yIndent, textWidth, getHeight() - yIndent * 2, colour, Semibold, 12);

            if (objectDescription.isNotEmpty()) {
                auto textLength = Fonts::getSemiBoldFont().withHeight(12).getStringWidth(getButtonText());

                leftIndent += textLength;
                auto textWidth = getWidth() - leftIndent - rightIndent;

                // Draw seperator (which is an en dash)
                PlugDataLook::drawText(g, String::fromUTF8("  \xe2\x80\x93  ") + objectDescription, Rectangle<int>(leftIndent, yIndent, textWidth, getHeight() - yIndent * 2), colour, 12);
            }

            if (type == -1)
                return;

            if (drawIcon) {
                auto dataColour = findColour(PlugDataColour::dataColourId);
                auto signalColour = findColour(PlugDataColour::signalColourId);
                g.setColour(type ? signalColour : dataColour);
                auto iconbound = getLocalBounds().reduced(4);
                iconbound.setWidth(getHeight() - 8);
                iconbound.translate(6, 0);
                g.fillRoundedRectangle(iconbound.toFloat(), PlugDataLook::smallCornerRadius);

                PlugDataLook::drawFittedText(g, type ? "~" : "pd", iconbound.reduced(1), Colours::white, 1, 1.0f, type ? 12 : 10, Justification::centred);
            }
        }

        SuggestionComponent* parent;
        bool drawIcon = true;
    };

public:

    SuggestionComponent()
        : resizer(this, &constrainer)
        , currentBox(nullptr)
        , dropShadower(DropShadow(Colour(0, 0, 0).withAlpha(0.25f), 7, { 0, 2 }))
    {
        // Set up the button list that contains our suggestions
        buttonholder = std::make_unique<Component>();

        if (Desktop::canUseSemiTransparentWindows()) {
            dropShadower.setOwner(this);
        }

        for (int i = 0; i < 20; i++) {
            Suggestion* but = buttons.add(new Suggestion(this, i));
            buttonholder->addAndMakeVisible(buttons[i]);

            but->setClickingTogglesState(true);
            but->setRadioGroupId(110);
            but->setColour(TextButton::buttonColourId, findColour(PlugDataColour::dialogBackgroundColourId));
        }

        // Hack for maintaining keyboard focus when resizing
        resizer.addMouseListener(this, true);

        // Set up viewport
        port = std::make_unique<Viewport>();
        port->setScrollBarsShown(true, false);
        port->setViewedComponent(buttonholder.get(), false);
        port->setInterceptsMouseClicks(true, true);
        port->setViewportIgnoreDragFlag(true);
        addAndMakeVisible(port.get());

        constrainer.setSizeLimits(150, 120, 500, 400);
        setSize(300, 140);

        addAndMakeVisible(resizer);

        setInterceptsMouseClicks(true, true);
        setAlwaysOnTop(true);
        setWantsKeyboardFocus(false);
    }

    ~SuggestionComponent() override
    {
        buttons.clear();
    }

    void createCalloutBox(Object* object, TextEditor* editor)
    {
        currentBox = object;
        openedEditor = editor;

        setTransform(object->cnv->editor->getTransform());

        editor->addListener(this);
        editor->addKeyListener(this);

        for (int i = 0; i < buttons.size(); i++) {
            auto* but = buttons[i];
            but->setAlwaysOnTop(true);

            but->onClick = [this, i, editor]() mutable {
                move(0, i);
                if (!editor->isVisible())
                    editor->setVisible(true);
                editor->grabKeyboardFocus();
            };
        }

        addToDesktop(ComponentPeer::windowIsTemporary | ComponentPeer::windowIgnoresKeyPresses);

        auto objectPos = object->getScreenBounds().reduced(Object::margin).getBottomLeft().translated(0, 5);

        setTopLeftPosition(objectPos);
        
        autoCompleteComponent = std::make_unique<AutoCompleteComponent>(editor, object->cnv);

        setVisible(false);
        toFront(false);

        repaint();
    }

    void removeCalloutBox()
    {
        setVisible(false);

        if (isOnDesktop()) {
            removeFromDesktop();
        }
        
        autoCompleteComponent.reset(nullptr);
        openedEditor->removeListener(this);

        openedEditor = nullptr;
        currentBox = nullptr;
    }

    void move(int offset, int setto = -1)
    {
        if (!openedEditor)
            return;

        // Calculate new selected index
        if (setto == -1)
            currentidx += offset;
        else
            currentidx = setto;

        if (numOptions == 0)
            return;

        // Limit it to minimum of the number of buttons and the number of suggestions
        int numButtons = std::min(20, numOptions);
        currentidx = (currentidx + numButtons) % numButtons;

        auto* but = buttons[currentidx];

        but->setToggleState(true, dontSendNotification);

        if (autoCompleteComponent) {
            String newText = buttons[currentidx]->getButtonText();
            autoCompleteComponent->setSuggestion(newText);
            currentBox->updateBounds();
        }

        // Auto-scroll item into viewport bounds
        if (port->getViewPositionY() > but->getY()) {
            port->setViewPosition(0, but->getY() - 6);
        } else if (port->getViewPositionY() + port->getMaximumVisibleHeight() < but->getY() + but->getHeight()) {
            port->setViewPosition(0, but->getY() - (but->getHeight() * 4) + 6);
        }

        repaint();
    }

    void resized() override
    {
        int yScroll = port->getViewPositionY();
        port->setBounds(getLocalBounds());
        buttonholder->setBounds(6, 0, getWidth(), std::min(numOptions, 20) * 25 + 8);

        for (int i = 0; i < buttons.size(); i++)
            buttons[i]->setBounds(2, (i * 25) + 4, getWidth() - 4, 24);

        int const resizerSize = 12;

        resizer.setBounds(getWidth() - (resizerSize + 1), getHeight() - (resizerSize + 1), resizerSize, resizerSize);

        port->setViewPosition(0, yScroll);
        repaint();
    }
        
    String getText() const {
        if(autoCompleteComponent) {
            return autoCompleteComponent->getSuggestion();
        }
        
        return String();
    }

private:
    void mouseDown(MouseEvent const& e) override
    {
        if (openedEditor)
            openedEditor->grabKeyboardFocus();
    }

    void paint(Graphics& g) override
    {

#if PLUGDATA_STANDALONE
        if (!Desktop::canUseSemiTransparentWindows()) {
            g.fillAll(findColour(PlugDataColour::canvasBackgroundColourId));
        }
#else
        auto hostType = PluginHostType();
        if (hostType.isLogic() || hostType.isGarageBand() || hostType.isMainStage()) {
            g.fillAll(findColour(PlugDataColour::canvasBackgroundColourId));
        }

#endif

        g.setColour(findColour(PlugDataColour::popupMenuBackgroundColourId));
        g.fillRoundedRectangle(port->getBounds().reduced(1).toFloat(), PlugDataLook::defaultCornerRadius);
    }

    void paintOverChildren(Graphics& g) override
    {
        g.setColour(findColour(PlugDataColour::outlineColourId).darker(0.1f));
        g.drawRoundedRectangle(port->getBounds().toFloat().reduced(0.5f), PlugDataLook::defaultCornerRadius, 1.0f);
    }

    bool keyPressed(KeyPress const& key, Component* originatingComponent) override
    {
        if (!currentBox) {
            return false;
        }

        if (key == KeyPress::rightKey && !openedEditor->getHighlightedRegion().isEmpty()) {
            openedEditor->setCaretPosition(openedEditor->getHighlightedRegion().getEnd());
            return true;
        }
        if(key == KeyPress::rightKey && autoCompleteComponent) {
            autoCompleteComponent->autocomplete();
            return true;
        }
        if (key == KeyPress::leftKey && !openedEditor->getHighlightedRegion().isEmpty()) {
            openedEditor->setCaretPosition(openedEditor->getHighlightedRegion().getStart());
            return true;
        }
        if (key == KeyPress::tabKey && autoCompleteComponent) {
            autoCompleteComponent->autocomplete();
            return true;
        }
        if (state != ShowingObjects)
            return false;

        if (key == KeyPress::upKey || key == KeyPress::downKey) {
            move(key == KeyPress::downKey ? 1 : -1);
            return true;
        }
        return false;
    }
        
        /*
    String filterNewText(TextEditor& e, String const& newInput) override
    {
        if (!currentBox) {
            return newInput;
        }

        String mutableInput = newInput;

        // Find start of highlighted region
        // This is the start of the last auto-completion suggestion
        // This region will automatically be removed after this function because it's selected
        int start = e.getHighlightedRegion().getLength() > 0 ? e.getHighlightedRegion().getStart() : e.getText().length();

        // Reconstruct users typing
        
    } */
    
    void textEditorTextChanged(TextEditor& e) override
    {
        if (!currentBox) return;
            
        String currentText = e.getText();
        resized();

        auto& library = currentBox->cnv->pd->objectLibrary;

        // If there's a space, open arguments panel
        if (currentText.contains(" ")) {
            state = ShowingArguments;
            auto found = library.getArguments()[currentText.upToFirstOccurrenceOf(" ", false, false)];
            for (int i = 0; i < std::min<int>(buttons.size(), static_cast<int>(found.size())); i++) {
                auto& [type, description, init] = found[i];
                buttons[i]->setText(type, description, false);
                buttons[i]->setInterceptsMouseClicks(false, false);
                buttons[i]->setToggleState(false, dontSendNotification);
            }

            numOptions = static_cast<int>(found.size());

            for (int i = numOptions; i < buttons.size(); i++) {
                buttons[i]->setText("", "", false);
                buttons[i]->setToggleState(false, dontSendNotification);
            }

            setVisible(numOptions);
            currentidx = 0;
            if(autoCompleteComponent)  {
                autoCompleteComponent->setSuggestion("");
                currentBox->updateBounds();
            }

            resized();

            return;
        }

        buttons[currentidx]->setToggleState(true, dontSendNotification);

        // Update suggestions
        auto found = library.autocomplete(currentText.toStdString());

        // When hvcc mode is enabled, show only hvcc compatible objects
        if (static_cast<bool>(currentBox->cnv->editor->hvccMode.getValue())) {
            std::vector<std::pair<String, bool>> hvccObjectsFound;
            for (auto& object : found) {
                if (Object::hvccObjects.contains(object.first)) {
                    hvccObjectsFound.push_back(object);
                }
            }

            found = hvccObjectsFound;
        }

        numOptions = static_cast<int>(found.size());

        // Apply object name and descriptions to buttons
        for (int i = 0; i < std::min<int>(buttons.size(), numOptions); i++) {
            auto& [name, autocomplete] = found[i];

            auto descriptions = library.getObjectDescriptions();

            if (descriptions.find(name) != descriptions.end()) {
                buttons[i]->setText(name, descriptions[name], true);
            } else {
                buttons[i]->setText(name, "", true);
            }
            buttons[i]->setInterceptsMouseClicks(true, false);
        }

        for (int i = numOptions; i < buttons.size(); i++)
            buttons[i]->setText("", "", false);

        resized();

        // Get length of user-typed text
        int textlen = e.getText().length();

        if (found.empty() || textlen == 0) {
            state = Hidden;
            if(autoCompleteComponent) autoCompleteComponent->setSuggestion("");
            currentBox->updateBounds();
            setVisible(false);
            return;
        }

        // Limit it to minimum of the number of buttons and the number of suggestions
        int numButtons = std::min(20, numOptions);

        currentidx = (currentidx + numButtons) % numButtons;

        // Retrieve best suggestion
        auto const& fullName = found[currentidx].first;

        state = ShowingObjects;
        if (fullName.length() > textlen && autoCompleteComponent) {
            autoCompleteComponent->setSuggestion(fullName);
        }
        else {
            autoCompleteComponent->setSuggestion("");
        }
        
        // duplicate call to updateBounds :(
        currentBox->updateBounds();

        setVisible(true);
    }

    enum SugesstionState {
        Hidden,
        ShowingObjects,
        ShowingArguments
    };

    int numOptions = 0;
    int currentidx = 0;

    std::unique_ptr<AutoCompleteComponent> autoCompleteComponent;
    std::unique_ptr<Viewport> port;
    std::unique_ptr<Component> buttonholder;
    OwnedArray<Suggestion> buttons;

    ResizableCornerComponent resizer;
    ComponentBoundsConstrainer constrainer;

    StackDropShadower dropShadower;
    
    SugesstionState state = Hidden;
        
    TextEditor* openedEditor = nullptr;
    SafePointer<Object> currentBox;
};
