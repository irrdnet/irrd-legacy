JC = javac
CLASSES = IRRj.class IrrdView.class IrrdTextArea.class \
	IrrdTextAreaUI.class GetRequest.class \
	IrrdSearchPane.class EditorWindow.class \
	OptionsWindow.class OptionsData.class \
	LimitTextField.class RpslObject.class \
	CheckBoxList.class IrrjApplet.class \
	FindWindow.class ObjectWizard.class 

all:
	$(MAKE) $(CLASSES)

.SUFFIXES: .java .class

.java.class:
	$(JC) $(JCFLAGS) $<

default:
	$(MAKE) all

clear:
	/bin/rm -f *.class
	/bin/rm -f *~
	/bin/rm -f #*#

clean:
	$(MAKE) clear

