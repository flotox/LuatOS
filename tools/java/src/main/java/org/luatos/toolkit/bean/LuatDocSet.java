package org.luatos.toolkit.bean;

import java.util.LinkedList;
import java.util.List;

public class LuatDocSet {

    private String name;

    private List<LuatDocument> docs;

    public LuatDocSet(String name) {
        this.name = name;
        this.docs = new LinkedList<>();
    }

    public void addDoc(LuatDocument doc) {
        this.docs.add(doc);
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public List<LuatDocument> getDocs() {
        return docs;
    }

}